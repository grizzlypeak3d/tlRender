// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/TimelinePrivate.h>

#include <filesystem>

#include <tlRender/Timeline/Util.h>
#include <tlRender/Timeline/ZipPrivate.h>

#include <tlRender/IO/SeqIO.h>
#include <tlRender/IO/System.h>

#include <tlRender/Core/URL.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>
#include <ftk/Core/Time.h>

#include <opentimelineio/externalReference.h>
#include <opentimelineio/gap.h>
#include <opentimelineio/imageSequenceReference.h>
#include <opentimelineio/transition.h>
#include <algorithm>

namespace tl
{
    namespace
    {
        //! An absolute, normalized form of a media path, used only to compare
        //! paths that name the same file in different ways.
        std::string normalMediaPath(const ftk::Path& path)
        {
            std::filesystem::path out = std::filesystem::u8path(path.get());
            if (!out.is_absolute())
            {
                std::error_code ec;
                const std::filesystem::path abs = std::filesystem::absolute(out, ec);
                if (!ec)
                {
                    out = abs;
                }
            }
            return out.lexically_normal().u8string();
        }
    }

    namespace
    {
        const std::chrono::milliseconds timeout(5);

        // How often an otherwise idle timeline wakes to log itself, and so
        // how long it will wait for a request before looking again.
        const std::chrono::seconds logInterval(10);
        // How long a timeline without a thread waits for one of its own
        // requests before giving up on it.
        const std::chrono::seconds syncRequestTimeout(60);

        //! Get the OTIO spatial coordinates of a media reference. These are
        //! optional; media without them is laid out from the image size as
        //! before. The coordinates are returned as authored, in the OTIO
        //! coordinate system: unit-less and Y-up.
        std::optional<ftk::Box2F> getMediaReferenceBounds(
            const OTIO_NS::MediaReference* otioMediaReference)
        {
            std::optional<ftk::Box2F> out;
            if (otioMediaReference)
            {
                const auto bounds = otioMediaReference->available_image_bounds();
                if (bounds.has_value())
                {
                    const auto& min = bounds.value().min;
                    const auto& max = bounds.value().max;
                    out = ftk::Box2F(
                        ftk::V2F(min.x, min.y),
                        ftk::V2F(max.x, max.y));
                }
            }
            return out;
        }

        //! Get the OTIO spatial coordinates of a clip's active media
        //! reference.
        std::optional<ftk::Box2F> getClipBounds(const OTIO_NS::Clip* otioClip)
        {
            return getMediaReferenceBounds(otioClip->media_reference());
        }

        //! Look on disk for the frames a sequence has and group them into runs
        //! of consecutive numbers, one per clip.
        //!
        //! This is the one place that goes looking, and it is a snapshot:
        //! frames written after it are picked up by opening the sequence again,
        //! not while it is being watched. The other policies need no such thing
        //! because they answer for a missing frame as they meet it.
        std::vector<ftk::RangeI64> getRuns(
            const ftk::Path& path,
            const ftk::RangeI64& within,
            const ftk::PathOptions& pathOptions)
        {
            std::vector<ftk::RangeI64> out;
            auto frames = ftk::toFrames(ftk::findSeq(path, pathOptions));
            std::sort(frames.begin(), frames.end());
            for (int64_t frame : frames)
            {
                if (frame < within.min() || frame > within.max())
                {
                    // Outside the range asked for, so not this sequence's
                    // business even though it sits beside it on disk.
                    continue;
                }
                if (!out.empty() && out.back().max() + 1 == frame)
                {
                    out.back() = ftk::RangeI64(out.back().min(), frame);
                }
                else
                {
                    out.push_back(ftk::RangeI64(frame, frame));
                }
            }
            return out;
        }

        //! Get the union of the OTIO spatial coordinates of every media
        //! reference on a clip. The canvas is built from this rather than from
        //! the active reference, so that changing the active media reference
        //! leaves the canvas unchanged.
        std::optional<ftk::Box2F> getClipBoundsUnion(const OTIO_NS::Clip* otioClip)
        {
            std::optional<ftk::Box2F> out;
            for (const auto& i : otioClip->media_references())
            {
                if (const auto bounds = getMediaReferenceBounds(i.second))
                {
                    out = out.has_value() ?
                        ftk::expand(out.value(), bounds.value()) :
                        bounds.value();
                }
            }
            return out;
        }

        //! Convert OTIO spatial coordinates into image space.
        //!
        //! The OTIO coordinates are unit-less, so they are scaled by the
        //! pixels per unit established from the first clip that has them;
        //! bounds of "0, 0, 1920, 1080" and "0, 0, 16, 9" describe the same
        //! area and must give the same result. The Y axis is also flipped,
        //! since OTIO is Y-up and image space is Y-down.
        std::optional<ftk::Box2F> toImageSpace(
            const std::optional<ftk::Box2F>& bounds,
            double scale)
        {
            std::optional<ftk::Box2F> out;
            if (bounds.has_value())
            {
                const auto& min = bounds.value().min;
                const auto& max = bounds.value().max;
                out = ftk::Box2F(
                    ftk::V2F(min.x * scale, -max.y * scale),
                    ftk::V2F(max.x * scale, -min.y * scale));
            }
            return out;
        }

        //! Resolve which media reference a clip should be read from.
        //!
        //! A key set for the clip alone takes precedence over the timeline
        //! wide key. Clips that do not have the requested key fall back to the
        //! default media key, and then to the media reference OTIO has active.
        OTIO_NS::MediaReference* resolveMediaReference(
            const OTIO_NS::Clip* otioClip,
            const std::string& key,
            const std::map<const OTIO_NS::Clip*, std::string>& clipKeys)
        {
            std::string clipKey = key;
            const auto i = clipKeys.find(otioClip);
            if (i != clipKeys.end() && !i->second.empty())
            {
                clipKey = i->second;
            }
            if (clipKey.empty())
            {
                // The common case, where no key has been set. Return early so
                // that the media reference map is not copied.
                return otioClip->media_reference();
            }
            const auto mediaReferences = otioClip->media_references();
            auto j = mediaReferences.find(clipKey);
            if (j == mediaReferences.end())
            {
                j = mediaReferences.find(OTIO_NS::Clip::default_media_key);
            }
            return j != mediaReferences.end() ?
                j->second :
                otioClip->media_reference();
        }

        //! Place a clip's spatial coordinates into image space.
        //!
        //! The bounds are passed in rather than read from the clip, since the
        //! caller decides whether they describe the media reference being read
        //! or the union of every reference on the clip.
        //!
        //! With Spatial::Normalize a clip that has no spatial coordinates is
        //! given the reference size, so that clips of differing resolutions
        //! are displayed at the same size. This covers timelines that were not
        //! authored with spatial coordinates at all.
        std::optional<ftk::Box2F> getSpatialBounds(
            const std::optional<ftk::Box2F>& clipBounds,
            Spatial spatial,
            const ftk::Size2I& normalizeSize,
            double scale)
        {
            std::optional<ftk::Box2F> out;
            if (Spatial::None == spatial)
            {
                return out;
            }
            out = toImageSpace(clipBounds, scale);
            if (!out.has_value() &&
                Spatial::Normalize == spatial &&
                normalizeSize.isValid())
            {
                out = ftk::Box2F(
                    ftk::V2F(0.F, -static_cast<float>(normalizeSize.h)),
                    ftk::V2F(static_cast<float>(normalizeSize.w), 0.F));
            }
            return out;
        }

        //! Get a clip's box within the timeline canvas.
        std::optional<ftk::Box2F> getCanvasBox(
            const std::optional<ftk::Box2F>& clipBounds,
            Spatial spatial,
            const ftk::Size2I& normalizeSize,
            double scale,
            const ftk::V2F& offset)
        {
            std::optional<ftk::Box2F> out;
            if (const auto bounds = getSpatialBounds(
                clipBounds,
                spatial,
                normalizeSize,
                scale))
            {
                out = bounds.value() + offset;
            }
            return out;
        }

        ftk::Path getAssociatedAudio(
            const std::shared_ptr<ftk::Context>& context,
            const ftk::Path& path,
            const ImageSeqAudio& imageSeqAudio,
            const std::vector<std::string>& imageSeqAudioExts,
            const std::string& imageSeqAudioFileName,
            const ftk::PathOptions& pathOptions)
        {
            ftk::Path out;
            auto ioSystem = context->getSystem<ReadSystem>();
            switch (imageSeqAudio)
            {
            case ImageSeqAudio::Ext:
            {
                // Check for an audio file with the same base name.
                std::vector<std::string> baseNames;
                baseNames.push_back(path.getDir() + path.getBase());
                std::string tmp = path.getBase();
                if (!tmp.empty() && '.' == tmp[tmp.size() - 1])
                {
                    tmp.pop_back();
                }
                baseNames.push_back(path.getDir() + tmp);
                for (const auto& baseName : baseNames)
                {
                    for (const auto& ext : imageSeqAudioExts)
                    {
                        const ftk::Path audioPath(baseName + ext, pathOptions);
                        if (std::filesystem::exists(std::filesystem::u8path(audioPath.get())))
                        {
                            out = audioPath;
                            break;
                        }
                    }
                }

                // Or use the first audio file.
                if (out.isEmpty())
                {
                    ftk::DirListOptions listOptions;
                    listOptions.filterExt = imageSeqAudioExts;
                    const auto entries = ftk::dirList(path.getDir(), listOptions);
                    if (!entries.empty())
                    {
                        out = entries.front().path;
                    }
                }

                break;
            }
            case ImageSeqAudio::FileName:
                out = ftk::Path(path.getDir() + imageSeqAudioFileName, pathOptions);
                break;
            default: break;
            }
            return out;
        }
    }

    void Timeline::_init(
        const std::shared_ptr<ftk::Context>& context,
        const ftk::Path& inputPath,
        const ftk::Path& inputAudioPath,
        const Options& options)
    {
        FTK_P();

        ftk::Path path = inputPath;
        ftk::Path audioPath = inputAudioPath;

        auto logSystem = context->getLogSystem();
        logSystem->print(
            "tl::Timeline::_init",
            ftk::Format(
                "\n"
                "    Path: {0}\n"
                "    Audio path: {1}").
            arg(path.get()).
            arg(audioPath.get()));

        OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otioTimeline;

        // Is the input a sequence?
        const std::vector<std::string> seqExts = getExts(
            context,
            static_cast<int>(FileType::Seq));
        const bool hasSeqExt = std::find(
            seqExts.begin(),
            seqExts.end(),
            ftk::toLower(path.getExt())) != seqExts.end();
        if (hasSeqExt && options.seqExpand)
        {
            path = ftk::expandSeq(path, options.pathOptions);
        }
        if (hasSeqExt && path.isSeq())
        {
            if (audioPath.isEmpty())
            {
                // Check for an associated audio file.
                audioPath = getAssociatedAudio(
                    context,
                    path,
                    options.imageSeqAudio,
                    options.imageSeqAudioExts,
                    options.imageSeqAudioFileName,
                    options.pathOptions);
            }
        }

        // Read the file. A sequence is read by a decoder, which holds no
        // thread; only a format that has to be read statefully still needs a
        // reader here.
        auto ioSystem = context->getSystem<ReadSystem>();
        IOInfo info;
        bool infoValid = false;
        if (auto plugin = ioSystem->getPlugin(path))
        {
            if (auto decode = plugin->decode(options.ioOptions))
            {
                info = SeqDecode::create(
                    path, {}, decode, options.ioOptions)->getInfo();
                infoValid = true;
            }
        }
        if (!infoValid)
        {
            // Which tracks this file gets depends on both halves, so both are
            // read and merged here. The readers are temporary: what the
            // timeline goes on to read is decided by the tracks below.
            auto videoRead = ioSystem->videoRead(path, options.ioOptions);
            auto audioRead = ioSystem->audioRead(path, options.ioOptions);
            std::future<IOInfo> videoFuture;
            std::future<IOInfo> audioFuture;
            if (videoRead)
            {
                videoFuture = videoRead->getInfo();
            }
            if (audioRead)
            {
                audioFuture = audioRead->getInfo();
            }
            IOInfo videoInfo;
            if (videoFuture.valid())
            {
                videoInfo = videoFuture.get();
                infoValid = true;
            }
            IOInfo audioInfo;
            if (audioFuture.valid())
            {
                audioInfo = audioFuture.get();
                infoValid = true;
            }
            if (infoValid)
            {
                info = merge(videoInfo, audioInfo);
            }
        }
        if (infoValid)
        {
            std::optional<OTIO_NS::RationalTime> startTime;
            OTIO_NS::Track* videoTrack = nullptr;
            OTIO_NS::Track* audioTrack = nullptr;

            // Read the video.
            if (!info.video.empty())
            {
                startTime = info.videoTime->start_time();
                const double rate = info.videoTime->duration().rate();
                const MissingFrames missingFrames =
                    getMissingFrames(options.ioOptions);

                // Every clip names the whole sequence over the whole range it
                // covers, whatever the clip itself takes out of it. They are
                // separate objects because a clip owns its reference, but they
                // describe the same file, so the reads behind them share one
                // decoder.
                const auto makeClip =
                    [&](const OTIO_NS::TimeRange& sourceRange)
                    {
                        auto out = new OTIO_NS::Clip;
                        out->set_source_range(sourceRange);
                        if (path.isSeq())
                        {
                            auto mediaReference =
                                new OTIO_NS::ImageSequenceReference(
                                    "",
                                    path.getBase(),
                                    path.getExt(),
                                    info.videoTime->start_time().value(),
                                    1,
                                    rate,
                                    path.getPad(),
                                    // A file opened directly has no reference
                                    // to say what to do about frames it is
                                    // missing, so it takes the options the
                                    // timeline was opened with.
                                    toOTIO(missingFrames));
                            mediaReference->set_available_range(*info.videoTime);
                            out->set_media_reference(mediaReference);
                        }
                        else
                        {
                            out->set_media_reference(
                                new OTIO_NS::ExternalReference(
                                    path.getFileName(),
                                    info.videoTime));
                        }
                        return out;
                    };

                // A structural policy is answered here rather than by the
                // reads: the frames that are there are found once, and a clip
                // is laid over each run of them. Skip puts the runs end to
                // end, so the timeline is only as long as the frames it has;
                // Gaps leaves the holes in, so every frame keeps the time it
                // had. Either way no read asks for a frame that is not there.
                std::vector<ftk::RangeI64> runs;
                if (path.isSeq() &&
                    isStructural(missingFrames) &&
                    path.getFrames().has_value())
                {
                    runs = getRuns(
                        path,
                        path.getFrames().value(),
                        options.pathOptions);
                }

                videoTrack = new OTIO_NS::Track(
                    "Video", std::nullopt, OTIO_NS::Track::Kind::video);
                if (runs.size() < 2 && MissingFrames::Gaps != missingFrames)
                {
                    // Nothing to take out, so this is the same single clip a
                    // complete sequence gets. A lone run still covers only
                    // itself, which is what Skip means when the frames that
                    // are there are consecutive.
                    videoTrack->append_child(makeClip(
                        runs.empty() ?
                        *info.videoTime :
                        OTIO_NS::TimeRange(
                            OTIO_NS::RationalTime(runs.front().min(), rate),
                            OTIO_NS::RationalTime(
                                runs.front().max() - runs.front().min() + 1,
                                rate))));
                }
                else
                {
                    const ftk::RangeI64& frames = path.getFrames().value();
                    int64_t at = frames.min();
                    for (const auto& run : runs)
                    {
                        if (MissingFrames::Gaps == missingFrames &&
                            run.min() > at)
                        {
                            videoTrack->append_child(new OTIO_NS::Gap(
                                OTIO_NS::RationalTime(run.min() - at, rate)));
                        }
                        videoTrack->append_child(makeClip(OTIO_NS::TimeRange(
                            OTIO_NS::RationalTime(run.min(), rate),
                            OTIO_NS::RationalTime(
                                run.max() - run.min() + 1, rate))));
                        at = run.max() + 1;
                    }
                    if (MissingFrames::Gaps == missingFrames &&
                        at <= frames.max())
                    {
                        // The tail of a render that has not got there yet, kept
                        // so the range asked for is the range shown.
                        videoTrack->append_child(new OTIO_NS::Gap(
                            OTIO_NS::RationalTime(frames.max() - at + 1, rate)));
                    }
                }
            }

            // Read the separate audio if provided.
            if (!audioPath.isEmpty())
            {
                if (auto audioRead = ioSystem->audioRead(audioPath, options.ioOptions))
                {
                    const auto audioInfo = audioRead->getInfo().get();

                    auto audioClip = new OTIO_NS::Clip;
                    audioClip->set_source_range(*audioInfo.audioTime);
                    audioClip->set_media_reference(new OTIO_NS::ExternalReference(
                        audioPath.getFileName(),
                        audioInfo.audioTime));

                    audioTrack = new OTIO_NS::Track("Audio", std::nullopt, OTIO_NS::Track::Kind::audio);
                    audioTrack->append_child(audioClip);
                }
            }
            else if (info.audio.isValid())
            {
                if (!startTime.has_value())
                {
                    startTime = info.audioTime->start_time();
                }

                auto audioClip = new OTIO_NS::Clip;
                audioClip->set_source_range(*info.audioTime);
                audioClip->set_media_reference(new OTIO_NS::ExternalReference(
                    path.getFileName(),
                    info.audioTime));

                audioTrack = new OTIO_NS::Track("Audio", std::nullopt, OTIO_NS::Track::Kind::audio);
                audioTrack->append_child(audioClip);
            }

            // Create the stack.
            auto otioStack = new OTIO_NS::Stack;
            if (videoTrack)
            {
                otioStack->append_child(videoTrack);
            }
            if (audioTrack)
            {
                otioStack->append_child(audioTrack);
            }

            // Create the timeline.
            otioTimeline = new OTIO_NS::Timeline(path.get());
            otioTimeline->set_tracks(otioStack);
            if (startTime.has_value())
            {
                otioTimeline->set_global_start_time(startTime);
            }
        }

        // Is the input an OTIO file?
        if (!otioTimeline)
        {
            const std::string fileName = path.get();
            const std::string ext = ftk::toLower(path.getExt());
            OTIO_NS::ErrorStatus otioError;
            if (".otio" == ext)
            {
                otioTimeline = dynamic_cast<OTIO_NS::Timeline*>(
                    OTIO_NS::Timeline::from_json_file(fileName, &otioError));
                if (!otioTimeline)
                {
                    throw std::runtime_error(
                        ftk::Format("Cannot read timeline: \"{0}\"").
                        arg(path.get()));
                }
                else if (OTIO_NS::is_error(otioError))
                {
                    throw std::runtime_error(
                        ftk::Format("Cannot read timeline: \"{0}\": {1}").
                        arg(path.get()).
                        arg(otioError.details));
                }
            }
            else if (".otioz" == ext)
            {
                // Read as scattered ranges rather than start to finish: opening
                // reads a local header per media file, and those are spread
                // across the whole bundle, one before each file's data. Asking
                // for sequential read ahead makes the operating system fetch
                // around every one of them and then throw it away.
                p.fileIO = ftk::FileIO::create(
                    fileName,
                    ftk::FileMode::Read,
                    ftk::FileRead::MMap,
                    ftk::FileAccess::Random);

                p.zipReader = std::make_shared<ZipReader>(logSystem);
                auto& zipReader = *p.zipReader;
                zipReader.open(fileName, p.fileIO->getSize());

                std::string json = zipReader.readText("content.otio");
                otioTimeline = dynamic_cast<OTIO_NS::Timeline*>(
                    OTIO_NS::Timeline::from_json_string(json, &otioError));
                if (!otioTimeline)
                {
                    throw std::runtime_error(
                        ftk::Format("Cannot read timeline: \"{0}\"").
                        arg(path.get()));
                }
                else if (OTIO_NS::is_error(otioError))
                {
                    throw std::runtime_error(
                        ftk::Format("Cannot read timeline: \"{0}\": {1}").
                        arg(path.get()).
                        arg(otioError.details));
                }

                // Map a media reference to the memory it occupies within the
                // bundle.
                //
                // The bundle is missing the media it is playing if the active
                // reference is not there, so that is an error. An alternate
                // that is missing only costs the ability to switch to it, so
                // the timeline is still opened and the reference is recorded
                // as unavailable. Either way the media is never read from its
                // path: a bundle is meant to be self contained, and quietly
                // reading a file from somewhere else would be misleading.
                // Record which references the bundle holds, and check the
                // first file of each so that a bundle missing its media is
                // still reported at open. Working out every frame's byte
                // range waits until the reference is read: for a bundle of
                // 25,000 frames that was seconds of URL decoding and path
                // parsing before anything appeared.
                const auto mapMediaReference = [&](
                    OTIO_NS::MediaReference* mediaReference,
                    bool active)
                {
                    if (!mediaReference ||
                        p.bundleMediaReferences.find(mediaReference) !=
                            p.bundleMediaReferences.end())
                    {
                        return;
                    }

                    std::string first;
                    if (auto externalReference =
                        dynamic_cast<OTIO_NS::ExternalReference*>(mediaReference))
                    {
                        first = ftk::Path(
                            decodeURL(externalReference->target_url())).get();
                    }
                    else if (auto imageSeqReference =
                        dynamic_cast<OTIO_NS::ImageSequenceReference*>(mediaReference))
                    {
                        if (imageSeqReference->number_of_images_in_sequence() <= 0)
                        {
                            return;
                        }
                        first = ftk::Path(decodeURL(
                            imageSeqReference->target_url_for_image_number(0))).get();
                    }
                    else
                    {
                        return;
                    }

                    if (!zipReader.find(first).has_value())
                    {
                        if (active)
                        {
                            throw std::runtime_error(ftk::Format(
                                "Cannot find zip entry: \"{0}\"").arg(first));
                        }
                        logSystem->print(
                            "tl::Timeline",
                            ftk::Format(
                                "Cannot find zip entry: \"{0}\"; this media "
                                "reference cannot be used").
                            arg(first),
                            ftk::LogType::Warning);
                        p.unavailableMediaReferences.insert(mediaReference);
                        return;
                    }
                    p.bundleMediaReferences.insert(mediaReference);
                };

                // Map every media reference, not only the active one, so that
                // the active reference can be changed without re-reading the
                // bundle.
                for (auto clip : otioTimeline->find_children<OTIO_NS::Clip>())
                {
                    const auto* activeReference = clip->media_reference();
                    for (const auto& i : clip->media_references())
                    {
                        mapMediaReference(i.second, i.second == activeReference);
                    }
                }
            }
        }

        if (!otioTimeline)
        {
            // Nothing claimed the file and it is not a timeline document.
            // Whether that is because the format is not supported at all or
            // because a supported file could not be read is the difference
            // between "try another application" and "this file is damaged",
            // so say which.
            throw std::runtime_error(
                ioSystem->getPlugin(path) ?
                ftk::Format("Cannot read the file: \"{0}\"").
                arg(path.get()).str() :
                ftk::Format("Unsupported file format: \"{0}\"").
                arg(path.get()).str());
        }

        OTIO_NS::AnyDictionary dict;
        dict["path"] = path.get();
        dict["audioPath"] = audioPath.get();
        otioTimeline->metadata()["tlRender"] = dict;

        _init(context, otioTimeline, options);
    }

    void Timeline::_init(
        const std::shared_ptr<ftk::Context>& context,
        const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline>& otioTimeline,
        const Options& options)
    {
        FTK_P();

        p.context = context;
        auto logSystem = context->getLogSystem();
        p.logSystem = logSystem;
        {
            std::vector<std::string> lines;
            lines.push_back(std::string());
            lines.push_back(ftk::Format("    * Image sequence audio: {0}").
                arg(options.imageSeqAudio));
            lines.push_back(ftk::Format("    * Image sequence audio extensions: {0}").
                arg(ftk::join(options.imageSeqAudioExts, ", ")));
            lines.push_back(ftk::Format("    * Image sequence audio file name: {0}").
                arg(options.imageSeqAudioFileName));
            lines.push_back(ftk::Format("    * Compatability: {0}").
                arg(options.compat));
            lines.push_back(ftk::Format("    * Read thread count: {0}").
                arg(options.readThreadCount));
            lines.push_back(ftk::Format("    * Audio request max: {0}").
                arg(options.audioRequestMax));
            for (const auto& i : options.ioOptions)
            {
                lines.push_back(ftk::Format("    * AV I/O {0}: {1}").
                    arg(i.first).
                    arg(i.second));
            }
            lines.push_back(ftk::Format("    * Path max number digits: {0}").
                arg(options.pathOptions.seqMaxDigits));
            logSystem->print(
                ftk::Format("tl::Timeline {0}").arg(this),
                ftk::join(lines, "\n"));
        }

        p.otioTimeline = otioTimeline;
        if (const auto i = otioTimeline->metadata().find("tlRender");
            i != otioTimeline->metadata().end())
        {
            try
            {
                const auto dict = std::any_cast<OTIO_NS::AnyDictionary>(i->second);
                if (auto j = dict.find("path"); j != dict.end())
                {
                    p.path = ftk::Path(std::any_cast<std::string>(j->second));
                }
                if (auto j = dict.find("audioPath"); j != dict.end())
                {
                    p.audioPath = ftk::Path(std::any_cast<std::string>(j->second));
                }
            }
            catch (const std::exception&)
            {}
        }
        p.options = options;
        p.videoReadCache.setMax(p.options.readCacheMax);
        p.audioReadCache.setMax(p.options.readCacheMax);
        p.seqCache.setMax(p.options.seqCacheMax);
        if (p.options.threaded)
        {
            p.startReadPool(p.options.readThreadCount);
        }

        // Get information about the timeline. A timeline whose tracks have
        // no duration is zero length rather than unset, so that everything
        // downstream has a range to work in.
        p.timeRange = tl::getTimeRange(p.otioTimeline.value).
            value_or(OTIO_NS::TimeRange());
        for (const auto& otioTrack :
            p.otioTimeline.value->find_children<OTIO_NS::Track>())
        {
            OTIO_NS::ErrorStatus errorStatus;
            const auto ranges = otioTrack->range_of_all_children(&errorStatus);
            if (OTIO_NS::is_error(errorStatus))
            {
                continue;
            }
            auto& trackItems = p.trackItems[otioTrack];
            for (const auto& i : ranges)
            {
                if (const auto trimmed = otioTrack->trim_child_range(i.second))
                {
                    p.trimmedRangeInParent[i.first] = trimmed.value();
                    if (auto otioItem = dynamic_cast<OTIO_NS::Item*>(i.first))
                    {
                        trackItems.push_back({ otioItem, trimmed.value() });
                    }
                }
            }
            std::sort(
                trackItems.begin(),
                trackItems.end(),
                [](const Private::TrackItem& a, const Private::TrackItem& b)
                {
                    return a.range.start_time() < b.range.start_time();
                });
        }
        for (const auto& otioClip :
            p.otioTimeline.value->find_children<OTIO_NS::Clip>())
        {
            for (const auto& i : otioClip->media_references())
            {
                if (i.second)
                {
                    const ftk::Path mediaPath = tl::getPath(
                        i.second,
                        p.path.getDir(),
                        p.options.pathOptions);
                    p.mediaByPath[mediaPath.get()] = i.second;
                    p.mediaByNormalPath[normalMediaPath(mediaPath)] = i.second;
                }
            }
        }
        for (const auto& i : p.otioTimeline.value->tracks()->children())
        {
            if (auto otioTrack = dynamic_cast<const OTIO_NS::Track*>(i.value))
            {
                if (OTIO_NS::Track::Kind::audio == otioTrack->kind())
                {
                    if (_getAudioInfo(otioTrack))
                    {
                        auto j = p.options.ioOptions.find("FFmpeg/AudioChannelCount");
                        if (j == p.options.ioOptions.end())
                        {
                            p.options.ioOptions["FFmpeg/AudioChannelCount"] =
                                ftk::Format("{0}").arg(p.ioInfo.audio.channelCount);
                        }
                        j = p.options.ioOptions.find("FFmpeg/AudioType");
                        if (j == p.options.ioOptions.end())
                        {
                            p.options.ioOptions["FFmpeg/AudioType"] =
                                ftk::Format("{0}").arg(p.ioInfo.audio.type);
                        }
                        j = p.options.ioOptions.find("FFmpeg/AudioSampleRate");
                        if (j == p.options.ioOptions.end())
                        {
                            p.options.ioOptions["FFmpeg/AudioSampleRate"] =
                                ftk::Format("{0}").arg(p.ioInfo.audio.sampleRate);
                        }
                        break;
                    }
                }
            }
        }
        for (const auto& i : p.otioTimeline.value->tracks()->children())
        {
            if (auto otioTrack = dynamic_cast<const OTIO_NS::Track*>(i.value))
            {
                if (OTIO_NS::Track::Kind::video == otioTrack->kind())
                {
                    if (_getVideoInfo(otioTrack))
                    {
                        break;
                    }
                }
            }
        }
        _getCanvas();

        // Give each media reference the timeline level information, keeping
        // its own video information and tags. getIOInfo() can then hand back
        // whichever of these matches the media reference being read.
        for (auto& i : p.videoInfoByReference)
        {
            IOInfo ioInfo = p.ioInfo;
            ioInfo.video = i.second.video;
            ioInfo.videoTime = i.second.videoTime;
            for (const auto& tag : i.second.tags)
            {
                ioInfo.tags[tag.first] = tag.second;
            }
            i.second = ioInfo;
        }

        // Reading the timeline opened readers, and one of them may already
        // have failed -- a media path that does not exist fails here. Errors
        // are otherwise collected as frame requests complete, and a timeline
        // whose media has no video is never asked for a frame, so without
        // this getReadError() would stay empty for exactly the media that
        // failed hardest. Safe to call here: the thread below is not running
        // yet.
        p.updateReadErrors();

        logSystem->print(
            ftk::Format("tl::Timeline {0}").arg(this),
            ftk::Format(
                "\n"
                "    * Time range: {0}\n"
                "    * Video: {1} {2}\n"
                "    * Audio: {3} {4} {5}").
            arg(p.timeRange).
            arg(!p.ioInfo.video.empty() ? p.ioInfo.video[0].size : ftk::Size2I()).
            arg(!p.ioInfo.video.empty() ? p.ioInfo.video[0].type : ftk::ImageType::None).
            arg(p.ioInfo.audio.channelCount).
            arg(p.ioInfo.audio.type).
            arg(p.ioInfo.audio.sampleRate));

        // Create a new thread.
        p.thread.running = true;
        p.thread.logTimer = std::chrono::steady_clock::now();
        if (p.options.threaded)
        {
            p.thread.thread = std::thread(
                [this]
                {
                    FTK_P();
                    while (p.thread.running)
                    {
                        _tick();
                    }
                    _finishRequests();
                });
        }
    }

    namespace
    {
        std::atomic<size_t> objectCount = 0;
    }

    Timeline::Timeline() :
        _p(new Private)
    {
        ++objectCount;
    }

    Timeline::~Timeline()
    {
        FTK_P();
        if (auto logSystem = p.logSystem.lock())
        {
            logSystem->print(
                ftk::Format("tl::~Timeline {0}").arg(this),
                p.path.get());
        }

        {
            std::unique_lock<std::mutex> lock(p.mutex.mutex);
            p.thread.running = false;
        }
        p.thread.cv.notify_one();
        if (p.thread.thread.joinable())
        {
            p.thread.thread.join();
        }
        p.stopReadPool();

        --objectCount;
    }

    std::shared_ptr<Timeline> Timeline::create(
        const std::shared_ptr<ftk::Context>& context,
        const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline>& timeline,
        const Options& options)
    {
        auto out = std::shared_ptr<Timeline>(new Timeline);
        out->_init(context, timeline, options);
        return out;
    }

    std::shared_ptr<Timeline> Timeline::create(
        const std::shared_ptr<ftk::Context>& context,
        const ftk::Path& path,
        const Options& options)
    {
        auto out = std::shared_ptr<Timeline>(new Timeline);
        out->_init(context, path, ftk::Path(), options);
        return out;
    }

    std::shared_ptr<Timeline> Timeline::create(
        const std::shared_ptr<ftk::Context>& context,
        const ftk::Path& path,
        const ftk::Path& audioPath,
        const Options& options)
    {
        auto out = std::shared_ptr<Timeline>(new Timeline);
        out->_init(context, path, audioPath, options);
        return out;
    }

    std::shared_ptr<Timeline> Timeline::create(
        const std::shared_ptr<ftk::Context>& context,
        const std::string& fileName,
        const Options& options)
    {
        auto out = std::shared_ptr<Timeline>(new Timeline);
        out->_init(
            context,
            ftk::Path(fileName, options.pathOptions),
            ftk::Path(),
            options);
        return out;
    }

    std::shared_ptr<Timeline> Timeline::create(
        const std::shared_ptr<ftk::Context>& context,
        const std::string& fileName,
        const std::string& audioFileName,
        const Options& options)
    {
        auto out = std::shared_ptr<Timeline>(new Timeline);
        out->_init(
            context,
            ftk::Path(fileName, options.pathOptions),
            ftk::Path(audioFileName, options.pathOptions),
            options);
        return out;
    }

    std::shared_ptr<ftk::Context> Timeline::getContext() const
    {
        return _p->context.lock();
    }
        
    const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline>& Timeline::getTimeline() const
    {
        return _p->otioTimeline;
    }

    const ftk::Path& Timeline::getPath() const
    {
        return _p->path;
    }

    const ftk::Path& Timeline::getAudioPath() const
    {
        return _p->audioPath;
    }

    const Options& Timeline::getOptions() const
    {
        return _p->options;
    }
    
    std::vector<ftk::MemFile> Timeline::getMem(const OTIO_NS::MediaReference* otioRef)
    {
        FTK_P();
        return *p.getMem(otioRef);
    }

    std::shared_ptr<std::vector<ftk::MemFile> > Timeline::Private::getMem(
        const OTIO_NS::MediaReference* otioRef)
    {
        std::unique_lock<std::mutex> lock(memFilesMutex);
        if (const auto i = memFiles.find(otioRef); i != memFiles.end())
        {
            return i->second;
        }
        if (bundleMediaReferences.find(otioRef) ==
            bundleMediaReferences.end())
        {
            // Not in a bundle: read from its path.
            return std::make_shared<std::vector<ftk::MemFile> >();
        }

        // First use of this reference: work out where each of its files lives
        // inside the bundle.
        //
        // A sequence member is placed at its offset from the first frame
        // rather than packed against the previous one, so that the result is
        // indexed by frame number. Packing them would misplace every frame
        // after a gap, and every frame at all when the step is greater than
        // one.
        auto out = std::make_shared<std::vector<ftk::MemFile> >();
        std::vector<std::pair<size_t, std::string> > mediaFileNames;
        if (auto externalReference = dynamic_cast<const OTIO_NS::ExternalReference*>(otioRef))
        {
            mediaFileNames.push_back(std::make_pair(
                size_t(0),
                ftk::Path(decodeURL(externalReference->target_url())).get()));
        }
        else if (auto imageSeqReference =
            dynamic_cast<const OTIO_NS::ImageSequenceReference*>(otioRef))
        {
            const int count = imageSeqReference->number_of_images_in_sequence();
            const size_t step = std::max(imageSeqReference->frame_step(), 1);
            mediaFileNames.reserve(count);
            for (int number = 0; number < count; ++number)
            {
                mediaFileNames.push_back(std::make_pair(
                    number * step,
                    ftk::Path(decodeURL(
                        imageSeqReference->target_url_for_image_number(number))).get()));
            }
        }
        if (!mediaFileNames.empty())
        {
            out->resize(mediaFileNames.back().first + 1);
        }
        size_t found = 0;
        std::string missing;
        size_t missingCount = 0;
        for (const auto& mediaFileName : mediaFileNames)
        {
            const auto entry = zipReader->find(mediaFileName.second);
            if (!entry.has_value())
            {
                // A sequence member the bundle does not hold is a missing
                // frame, which the sequence decoder deals with. Leave its slot
                // empty and carry on.
                ++missingCount;
                if (missing.empty())
                {
                    missing = mediaFileName.second;
                }
                continue;
            }
            (*out)[mediaFileName.first] = ftk::MemFile(
                fileIO,
                fileIO->getMemStart() + entry->offset,
                entry->size);
            ++found;
        }
        if (0 == found)
        {
            // The bundle holds none of this media. Mark the reference
            // unavailable rather than returning nothing: an empty result reads
            // as "not in a bundle", and the caller would go on to read the
            // media from its path, which is a different file than the bundle
            // describes.
            if (auto log = logSystem.lock())
            {
                log->print(
                    "tl::Timeline",
                    ftk::Format(
                        "Cannot find zip entry: \"{0}\"; this media "
                        "reference cannot be used").arg(missing),
                    ftk::LogType::Error);
            }
            unavailableMediaReferences.insert(otioRef);
            out->clear();
        }
        else if (missingCount > 0)
        {
            if (auto log = logSystem.lock())
            {
                log->print(
                    "tl::Timeline",
                    ftk::Format(
                        "Bundle is missing {0} of {1} sequence frames, "
                        "starting with \"{2}\"").
                        arg(missingCount).
                        arg(mediaFileNames.size()).
                        arg(missing),
                    ftk::LogType::Warning);
            }
        }
        memFiles[otioRef] = out;
        return out;
    }

    OTIO_NS::MediaReference* Timeline::Private::mediaReference(
        const OTIO_NS::Clip* otioClip) const
    {
        return resolveMediaReference(
            otioClip,
            thread.mediaReferenceKey,
            thread.clipMediaReferenceKeys);
    }

    std::optional<OTIO_NS::TimeRange>
        Timeline::Private::getTrimmedRangeInParent(
            const OTIO_NS::Composable* otioComposable) const
    {
        if (const auto i = trimmedRangeInParent.find(otioComposable);
            i != trimmedRangeInParent.end())
        {
            return i->second;
        }
        if (auto otioItem = dynamic_cast<const OTIO_NS::Item*>(otioComposable))
        {
            return otioItem->trimmed_range_in_parent();
        }
        return std::nullopt;
    }

    std::vector<OTIO_NS::Composable*> Timeline::Private::getTrackChildrenAt(
        const OTIO_NS::Track* otioTrack,
        const OTIO_NS::RationalTime& time) const
    {
        std::vector<OTIO_NS::Composable*> out;
        const auto i = trackItems.find(otioTrack);
        if (i == trackItems.end())
        {
            for (const auto& otioChild : otioTrack->children())
            {
                out.push_back(otioChild.value);
            }
            return out;
        }
        const auto& items = i->second;
        auto j = std::upper_bound(
            items.begin(),
            items.end(),
            time,
            [](const OTIO_NS::RationalTime& value, const TrackItem& item)
            {
                return value < item.range.start_time();
            });
        if (j != items.begin())
        {
            --j;
            if (j->range.contains(time))
            {
                out.push_back(j->item);
            }
        }
        return out;
    }

    namespace
    {
        // Every media reference in the timeline, active or not, so that a
        // caller can name one that is not the one currently being played.
    }

    size_t Timeline::getVideoRequestMax() const
    {
        // At least one, whatever the options say. Zero here would not mean
        // "no limit", it would mean no request is ever picked up, and a
        // timeline without a thread would wait for one that never came.
        return std::max(_p->options.readThreadCount, size_t(1)) * 2;
    }

    size_t Timeline::getReadThreadCount() const
    {
        return _p->readPool.threads.size();
    }

    std::vector<ftk::Path> Timeline::getMediaPaths() const
    {
        FTK_P();
        std::vector<ftk::Path> out;
        for (const auto& i : p.mediaByPath)
        {
            out.push_back(ftk::Path(i.first));
        }
        return out;
    }

    OTIO_NS::MediaReference* Timeline::_findMedia(const ftk::Path& path)
    {
        FTK_P();
        const auto i = p.mediaByPath.find(path.get());
        if (i != p.mediaByPath.end())
        {
            return i->second;
        }
        // The media references are resolved when the timeline is read, so they
        // are usually absolute, while a caller asks with the path it was given.
        // Opening a file by a relative path otherwise found none of its own
        // media.
        const auto j = p.mediaByNormalPath.find(normalMediaPath(path));
        return j != p.mediaByNormalPath.end() ? j->second : nullptr;
    }

    bool Timeline::getMediaInfo(
        const ftk::Path& path,
        IOInfo& out,
        const IOOptions& options)
    {


        FTK_P();
        if (auto mediaReference = _findMedia(path))
        {
            return _getIOInfo(
                mediaReference, merge(options, p.options.ioOptions), out);
        }
        return false;
    }

    std::optional<Timeline::MediaAt> Timeline::_mediaAt(
        const OTIO_NS::RationalTime& time)
    {
        FTK_P();
        std::optional<MediaAt> out;
        if (!p.otioTimeline)
        {
            return out;
        }
        // The same lookup the request thread makes: the timeline's own start is
        // taken off, and the first enabled video track holding that time wins.
        // Bisected rather than walked, because this is asked for the playhead
        // and for every ruler label that is drawn, and a sequence built out of
        // the runs of frames it has can be in a great many pieces.
        const OTIO_NS::RationalTime trackTime = time - p.timeRange.start_time();
        for (const auto& otioTrack : p.otioTimeline->video_tracks())
        {
            if (!otioTrack->enabled())
            {
                continue;
            }
            for (const auto& otioChild :
                p.getTrackChildrenAt(otioTrack, trackTime))
            {
                auto otioClip = dynamic_cast<const OTIO_NS::Clip*>(otioChild);
                if (!otioClip)
                {
                    continue;
                }
                const auto rangeInParent = p.getTrimmedRangeInParent(otioClip);
                if (!rangeInParent.has_value() ||
                    !rangeInParent.value().contains(trackTime))
                {
                    continue;
                }

                out = _mediaFrom(otioClip, rangeInParent.value());
                if (out)
                {
                    return out;
                }
            }
        }
        return out;
    }

    std::optional<Timeline::MediaAt> Timeline::_mediaFrom(
        const OTIO_NS::Clip* otioClip,
        const OTIO_NS::TimeRange& rangeInParent)
    {
        FTK_P();
        std::optional<MediaAt> out;
        const IOOptions optionsMerged = p.options.ioOptions;
        auto mediaReference = p.mediaReference(otioClip);
        MediaAt mediaAt;
        mediaAt.seq = _getSeqDecode(mediaReference, optionsMerged);
        IOInfo ioInfo;
        if (mediaAt.seq)
        {
            ioInfo = mediaAt.seq->getInfo();
        }
        else if (auto read = _getVideoRead(mediaReference, optionsMerged))
        {
            ioInfo = read->getInfo().get();
        }
        else
        {
            return out;
        }

        if (!ioInfo.videoTime.has_value())
        {
            // No video in the media, so there is no rate to convert times
            // with and nothing to say where the clip sits.
            return out;
        }
        OTIO_NS::TimeRange trimmedRange = otioClip->trimmed_range();
        const OTIO_NS::TimeRange availableRange = otioClip->available_range();
        if (p.options.compat &&
            availableRange.start_time() > ioInfo.videoTime->start_time())
        {
            // The same compensation _readVideo() makes, so that both agree on
            // which media time a timeline time means.
            trimmedRange = OTIO_NS::TimeRange(
                trimmedRange.start_time() - availableRange.start_time(),
                trimmedRange.duration());
        }
        mediaAt.rangeInParent = rangeInParent;
        mediaAt.trimmedRange = trimmedRange;
        mediaAt.rate = ioInfo.videoTime->duration().rate();
        out = mediaAt;
        return out;
    }

    std::vector<Timeline::MediaAt> Timeline::_mediaAll()
    {
        FTK_P();
        std::vector<MediaAt> out;
        if (!p.otioTimeline)
        {
            return out;
        }
        for (const auto& otioTrack : p.otioTimeline->video_tracks())
        {
            if (!otioTrack->enabled())
            {
                continue;
            }
            // Every clip, not just the one at some time: this is for finding
            // which clip holds a frame that was asked for by number. The ranges
            // still come from the index rather than from OTIO.
            for (const auto& otioChild : otioTrack->children())
            {
                auto otioClip = dynamic_cast<const OTIO_NS::Clip*>(otioChild.value);
                if (!otioClip)
                {
                    continue;
                }
                const auto rangeInParent = p.getTrimmedRangeInParent(otioClip);
                if (!rangeInParent.has_value())
                {
                    continue;
                }
                if (const auto mediaAt =
                    _mediaFrom(otioClip, rangeInParent.value()))
                {
                    out.push_back(*mediaAt);
                }
            }
        }
        return out;
    }

    OTIO_NS::RationalTime Timeline::_toMediaTime(
        const MediaAt& mediaAt,
        const OTIO_NS::RationalTime& time) const
    {
        FTK_P();
        return toVideoMediaTime(
            time - p.timeRange.start_time(),
            mediaAt.rangeInParent,
            mediaAt.trimmedRange,
            mediaAt.rate);
    }

    OTIO_NS::RationalTime Timeline::_fromMediaTime(
        const MediaAt& mediaAt,
        int64_t frame) const
    {
        FTK_P();

        // The inverse of toVideoMediaTime(), with the timeline's own start put
        // back on. Which frames a clip covers is the caller's business: this
        // just moves a frame number into the clip that holds it.
        const OTIO_NS::RationalTime mediaTime(
            static_cast<double>(frame), mediaAt.rate);
        return (mediaTime
            - mediaAt.trimmedRange.start_time()
            + mediaAt.rangeInParent.start_time()
            + p.timeRange.start_time()).
            rescaled_to(mediaAt.rangeInParent.duration().rate()).
            round();
    }

    std::optional<OTIO_NS::RationalTime> Timeline::getMediaTime(
        const OTIO_NS::RationalTime& time)
    {
        std::optional<OTIO_NS::RationalTime> out;
        if (const auto mediaAt = _mediaAt(time))
        {
            out = _toMediaTime(*mediaAt, time);
        }
        return out;
    }

    std::optional<int64_t> Timeline::getMediaFrame(
        const OTIO_NS::RationalTime& time)
    {
        std::optional<int64_t> out;
        if (const auto mediaTime = getMediaTime(time))
        {
            // Already whole, at the media's rate.
            out = static_cast<int64_t>(mediaTime->value());
        }
        return out;
    }

    std::optional<OTIO_NS::RationalTime> Timeline::getMediaFrameTime(
        const OTIO_NS::RationalTime& time,
        int64_t frame)
    {
        std::optional<OTIO_NS::RationalTime> out;
        if (const auto mediaAt = _mediaAt(time))
        {
            out = _fromMediaTime(*mediaAt, frame);
        }
        return out;
    }

    std::optional<OTIO_NS::RationalTime> Timeline::getTimelineTime(
        const OTIO_NS::RationalTime& time,
        const OTIO_NS::RationalTime& mediaTime)
    {
        std::optional<OTIO_NS::RationalTime> out;
        const auto at = _mediaAt(time);
        if (!at)
        {
            return out;
        }
        const OTIO_NS::RationalTime frame =
            mediaTime.rescaled_to(at->rate).round();

        // The clip being looked at, when it is the one holding the frame asked
        // for. This is the whole answer for a timeline whose clips cover their
        // media without a break.
        if (at->trimmedRange.contains(frame))
        {
            out = _fromMediaTime(
                *at, static_cast<int64_t>(frame.value()));
            return out;
        }

        // Otherwise the frame belongs to one of the other clips over the same
        // media, which is what a sequence with frames left out looks like. Only
        // clips over that same media are considered, so a frame number means
        // the same thing it does in the clip it was typed against rather than
        // being matched against some other file that happens to number its
        // frames the same way.
        std::optional<MediaAt> snap;
        for (const auto& i : _mediaAll())
        {
            if (i.seq != at->seq)
            {
                continue;
            }
            if (i.trimmedRange.contains(frame))
            {
                out = _fromMediaTime(i, static_cast<int64_t>(frame.value()));
                return out;
            }
            const bool before = i.trimmedRange.end_time_inclusive() < frame;
            if (before &&
                (!snap ||
                    snap->trimmedRange.end_time_inclusive() <
                    i.trimmedRange.end_time_inclusive()))
            {
                snap = i;
            }
        }

        // A frame that is not there at all snaps to the last one before it, or
        // to the first frame when it is before them all, so that typing a
        // number always lands somewhere.
        if (snap)
        {
            out = _fromMediaTime(
                *snap,
                static_cast<int64_t>(
                    snap->trimmedRange.end_time_inclusive().value()));
        }
        else
        {
            out = _fromMediaTime(
                *at,
                static_cast<int64_t>(at->trimmedRange.start_time().value()));
        }
        return out;
    }

    std::future<VideoData> Timeline::readMedia(
        const ftk::Path& path,
        const OTIO_NS::RationalTime& time,
        const IOOptions& options)
    {
        FTK_P();
        std::future<VideoData> out;
        const IOOptions optionsMerged = merge(options, p.options.ioOptions);
        if (auto mediaReference = _findMedia(path))
        {
            if (auto seq = _getSeqDecode(mediaReference, optionsMerged))
            {
                out = p.submitRead(
                    [seq, time, optionsMerged]
                    {
                        return seq->readVideo(time, optionsMerged);
                    });
            }
            else if (auto videoRead = _getVideoRead(mediaReference, optionsMerged))
            {
                out = videoRead->readVideo(time, optionsMerged);
            }
        }
        return out;
    }

    std::future<AudioData> Timeline::readMediaAudio(
        const ftk::Path& path,
        const OTIO_NS::TimeRange& timeRange,
        const IOOptions& options)
    {
        FTK_P();
        std::future<AudioData> out;
        const IOOptions optionsMerged = merge(options, p.options.ioOptions);
        if (auto mediaReference = _findMedia(path))
        {
            // Audio is never a sequence of stateless files.
            if (auto audioRead = _getAudioRead(mediaReference, optionsMerged))
            {
                out = audioRead->readAudio(timeRange, optionsMerged);
            }
        }
        return out;
    }

    std::vector<std::string> Timeline::getMediaReferenceKeys() const
    {
        FTK_P();
        std::set<std::string> keys;
        for (const auto& otioClip :
            p.otioTimeline.value->find_children<OTIO_NS::Clip>())
        {
            for (const auto& i : otioClip->media_references())
            {
                keys.insert(i.first);
            }
        }
        return std::vector<std::string>(keys.begin(), keys.end());
    }

    std::string Timeline::getMediaReferenceKey() const
    {
        FTK_P();
        std::unique_lock<std::mutex> lock(p.mutex.mutex);
        return p.mutex.mediaReferenceKey;
    }

    void Timeline::setMediaReferenceKey(const std::string& value)
    {
        FTK_P();
        std::unique_lock<std::mutex> lock(p.mutex.mutex);
        if (value != p.mutex.mediaReferenceKey)
        {
            p.mutex.mediaReferenceKey = value;
            p.mutex.mediaReferenceKeysChanged = true;
        }
    }

    std::string Timeline::getMediaReferenceKey(
        const OTIO_NS::Clip* otioClip) const
    {
        FTK_P();
        std::unique_lock<std::mutex> lock(p.mutex.mutex);
        const auto i = p.mutex.clipMediaReferenceKeys.find(otioClip);
        return i != p.mutex.clipMediaReferenceKeys.end() ?
            i->second :
            std::string();
    }

    void Timeline::setMediaReferenceKey(
        const OTIO_NS::Clip* otioClip,
        const std::string& value)
    {
        FTK_P();
        std::unique_lock<std::mutex> lock(p.mutex.mutex);
        if (value.empty())
        {
            if (p.mutex.clipMediaReferenceKeys.erase(otioClip) > 0)
            {
                p.mutex.mediaReferenceKeysChanged = true;
            }
        }
        else
        {
            auto& key = p.mutex.clipMediaReferenceKeys[otioClip];
            if (value != key)
            {
                key = value;
                p.mutex.mediaReferenceKeysChanged = true;
            }
        }
    }

    OTIO_NS::MediaReference* Timeline::getMediaReference(
        const OTIO_NS::Clip* otioClip) const
    {
        FTK_P();
        std::unique_lock<std::mutex> lock(p.mutex.mutex);
        return resolveMediaReference(
            otioClip,
            p.mutex.mediaReferenceKey,
            p.mutex.clipMediaReferenceKeys);
    }

    const OTIO_NS::TimeRange& Timeline::getTimeRange() const
    {
        return _p->timeRange;
    }

    OTIO_NS::RationalTime Timeline::getDuration() const
    {
        return _p->timeRange.duration();
    }

    const IOInfo& Timeline::getIOInfo() const
    {
        FTK_P();
        // Follow the media reference being read, so that the information
        // describes the media on screen rather than the media that happened to
        // be active when the timeline was read. The entries are fixed once the
        // timeline has been read, so returning a reference to one is safe.
        if (p.videoInfoClip)
        {
            const auto i = p.videoInfoByReference.find(
                getMediaReference(p.videoInfoClip));
            if (i != p.videoInfoByReference.end())
            {
                return i->second;
            }
        }
        return p.ioInfo;
    }

    std::string Timeline::getReadError() const
    {
        std::unique_lock<std::mutex> lock(_p->mutex.mutex);
        return _p->mutex.readError;
    }

    size_t Timeline::getReadErrorCount() const
    {
        std::unique_lock<std::mutex> lock(_p->mutex.mutex);
        return _p->mutex.readErrorCount;
    }

    VideoRequest Timeline::getVideo(
        const OTIO_NS::RationalTime& time,
        const IOOptions& options)
    {
        FTK_P();
        (p.requestId)++;
        auto request = std::make_shared<Private::PendingVideoRequest>();
        request->id = p.requestId;
        request->time = time;
        request->options = options;
        VideoRequest out;
        out.id = p.requestId;
        out.future = request->promise.get_future();
        bool valid = false;
        {
            std::unique_lock<std::mutex> lock(p.mutex.mutex);
            if (!p.mutex.stopped)
            {
                valid = true;
                p.mutex.videoRequests.push_back(request);
            }
        }
        if (valid)
        {
            p.thread.cv.notify_one();
        }
        else
        {
            request->promise.set_value(VideoFrame());
        }
        if (!p.options.threaded)
        {
            // Nothing else is going to run this request. Bounded because a
            // caller blocking on the future would have no way to find out
            // that it is never going to resolve; reaching the bound is a bug
            // here rather than a slow read.
            std::unique_lock<std::mutex> driver(p.driverMutex);
            const auto start = std::chrono::steady_clock::now();
            while (out.future.valid() &&
                out.future.wait_for(std::chrono::seconds(0)) !=
                    std::future_status::ready)
            {
                if (std::chrono::steady_clock::now() - start > syncRequestTimeout)
                {
                    p.abandon(request);
                    break;
                }
                _tick();
            }
        }
        return out;
    }

    AudioRequest Timeline::getAudio(
        double seconds,
        const IOOptions& options)
    {
        FTK_P();
        (p.requestId)++;
        auto request = std::make_shared<Private::PendingAudioRequest>();
        request->id = p.requestId;
        request->seconds = seconds;
        request->options = options;
        AudioRequest out;
        out.id = p.requestId;
        out.future = request->promise.get_future();
        bool valid = false;
        {
            std::unique_lock<std::mutex> lock(p.mutex.mutex);
            if (!p.mutex.stopped)
            {
                valid = true;
                p.mutex.audioRequests.push_back(request);
            }
        }
        if (valid)
        {
            p.thread.cv.notify_one();
        }
        else
        {
            request->promise.set_value(AudioFrame());
        }
        if (!p.options.threaded)
        {
            // Nothing else is going to run this request. Bounded because a
            // caller blocking on the future would have no way to find out
            // that it is never going to resolve; reaching the bound is a bug
            // here rather than a slow read.
            std::unique_lock<std::mutex> driver(p.driverMutex);
            const auto start = std::chrono::steady_clock::now();
            while (out.future.valid() &&
                out.future.wait_for(std::chrono::seconds(0)) !=
                    std::future_status::ready)
            {
                if (std::chrono::steady_clock::now() - start > syncRequestTimeout)
                {
                    p.abandon(request);
                    break;
                }
                _tick();
            }
        }
        return out;
    }

    void Timeline::cancelRequests(const std::vector<uint64_t>& ids)
    {
        FTK_P();
        auto cancel = [&ids](auto& requests)
        {
            auto i = requests.begin();
            while (i != requests.end())
            {
                if (std::find(ids.begin(), ids.end(), (*i)->id) != ids.end())
                {
                    i = requests.erase(i);
                }
                else
                {
                    ++i;
                }
            }
        };
        std::unique_lock<std::mutex> lock(p.mutex.mutex);
        cancel(p.mutex.videoRequests);
        cancel(p.mutex.audioRequests);
    }

    size_t Timeline::getObjectCount()
    {
        return objectCount;
    }

    namespace
    {
        std::string getKey(const ftk::Path& path)
        {
            std::vector<std::string> out;
            out.push_back(path.get());
            out.push_back(path.getNum());
            return ftk::join(out, ';');
        }
    }

    template<typename T>
    std::shared_ptr<T> Timeline::Private::getCached(
        ftk::LRUCache<std::string, std::shared_ptr<T> >& cache,
        const OTIO_NS::MediaReference* mediaReference,
        const IOOptions& ioOptions,
        const std::function<std::shared_ptr<T>(
            const std::shared_ptr<ftk::Context>&,
            const ftk::Path&,
            const std::vector<ftk::MemFile>&,
            const IOOptions&)>& create)
    {
        std::shared_ptr<T> out;
        if (mediaUnavailable(mediaReference))
        {
            // Named by the bundle but not inside it. Reading it from its
            // path would be reading a different file than the bundle
            // describes.
            return out;
        }
        const auto mediaPath = tl::getPath(
            mediaReference,
            path.getDir(),
            options.pathOptions);
        const std::string key = getKey(mediaPath);
        std::unique_lock<std::mutex> lock(readCacheMutex);
        if (!cache.get(key, out))
        {
            auto context = this->context.lock();
            if (!context)
            {
                return out;
            }
            try
            {
                const auto mem = getMem(mediaReference);
                if (mediaUnavailable(mediaReference))
                {
                    // Resolving its byte ranges said the bundle does not
                    // hold it; reading it from its path is not the same
                    // file.
                    return out;
                }
                IOOptions readOptions = ioOptions;
                readOptions["SeqIO/DefaultSpeed"] =
                    ftk::Format("{0}").arg(timeRange.duration().rate());
                if (auto imageSeqReference =
                    dynamic_cast<const OTIO_NS::ImageSequenceReference*>(mediaReference))
                {
                    // The reference says what to do about frames it does not
                    // have, and it is more specific than the options the
                    // timeline was opened with. A reference this timeline
                    // built for a file opened directly carries those options
                    // already.
                    readOptions["SeqIO/MissingFrames"] = to_string(
                        fromOTIO(imageSeqReference->missing_frame_policy()));
                }
                out = create(context, mediaPath, *mem, readOptions);
            }
            catch (const std::exception& e)
            {
                if (auto log = logSystem.lock())
                {
                    log->print(
                        "tl::Timeline",
                        ftk::Format("Cannot read \"{0}\": {1}").
                            arg(mediaPath.get()).arg(e.what()),
                        ftk::LogType::Error);
                }
                return std::shared_ptr<T>();
            }
            if (out)
            {
                cache.add(key, out);
            }
        }
        return out;
    }

    std::shared_ptr<SeqDecode> Timeline::_getSeqDecode(
        const OTIO_NS::MediaReference* mediaReference,
        const IOOptions& ioOptions)
    {
        FTK_P();
        return p.getCached<SeqDecode>(
            p.seqCache,
            mediaReference,
            ioOptions,
            [](const std::shared_ptr<ftk::Context>& context,
                const ftk::Path& path,
                const std::vector<ftk::MemFile>& mem,
                const IOOptions& options)
            {
                std::shared_ptr<SeqDecode> out;
                const auto readSystem = context->getSystem<ReadSystem>();
                if (const auto plugin = readSystem->getPlugin(path))
                {
                    // Null for a format that has to be read statefully,
                    // which leaves the caller to fall back to a reader.
                    if (const auto decode = plugin->decode(options))
                    {
                        out = SeqDecode::create(path, mem, decode, options);
                    }
                }
                return out;
            });
    }

    bool Timeline::_getVideoIOInfo(
        const OTIO_NS::MediaReference* mediaReference,
        const IOOptions& ioOptions,
        IOInfo& out)
    {
        if (auto seq = _getSeqDecode(mediaReference, ioOptions))
        {
            out = seq->getInfo();
            return true;
        }
        if (auto videoRead = _getVideoRead(mediaReference, ioOptions))
        {
            out = videoRead->getInfo().get();
            return true;
        }
        return false;
    }

    bool Timeline::_getAudioIOInfo(
        const OTIO_NS::MediaReference* mediaReference,
        const IOOptions& ioOptions,
        IOInfo& out)
    {
        // Audio is never a sequence of stateless files.
        if (auto audioRead = _getAudioRead(mediaReference, ioOptions))
        {
            out = audioRead->getInfo().get();
            return true;
        }
        return false;
    }

    bool Timeline::_getIOInfo(
        const OTIO_NS::MediaReference* mediaReference,
        const IOOptions& ioOptions,
        IOInfo& out)
    {
        if (auto seq = _getSeqDecode(mediaReference, ioOptions))
        {
            out = seq->getInfo();
            return true;
        }
        // Both requests go out before either is waited on, so that the two
        // readers open the file at the same time.
        auto videoRead = _getVideoRead(mediaReference, ioOptions);
        auto audioRead = _getAudioRead(mediaReference, ioOptions);
        std::future<IOInfo> videoFuture;
        std::future<IOInfo> audioFuture;
        if (videoRead)
        {
            videoFuture = videoRead->getInfo();
        }
        if (audioRead)
        {
            audioFuture = audioRead->getInfo();
        }
        if (!videoFuture.valid() && !audioFuture.valid())
        {
            return false;
        }
        IOInfo videoInfo;
        if (videoFuture.valid())
        {
            videoInfo = videoFuture.get();
        }
        IOInfo audioInfo;
        if (audioFuture.valid())
        {
            audioInfo = audioFuture.get();
        }
        out = merge(videoInfo, audioInfo);
        return true;
    }

    std::shared_ptr<IVideoRead> Timeline::_getVideoRead(
        const OTIO_NS::Clip* clip,
        const IOOptions& ioOptions)
    {
        FTK_P();
        return _getVideoRead(p.mediaReference(clip), ioOptions);
    }

    std::shared_ptr<IVideoRead> Timeline::_getVideoRead(
        const OTIO_NS::MediaReference* mediaReference,
        const IOOptions& ioOptions)
    {
        FTK_P();
        return p.getCached<IVideoRead>(
            p.videoReadCache,
            mediaReference,
            ioOptions,
            [](const std::shared_ptr<ftk::Context>& context,
                const ftk::Path& path,
                const std::vector<ftk::MemFile>& mem,
                const IOOptions& options)
            {
                return context->getSystem<ReadSystem>()->videoRead(
                    path, mem, options);
            });
    }

    std::shared_ptr<IAudioRead> Timeline::_getAudioRead(
        const OTIO_NS::Clip* clip,
        const IOOptions& ioOptions)
    {
        FTK_P();
        return _getAudioRead(p.mediaReference(clip), ioOptions);
    }

    std::shared_ptr<IAudioRead> Timeline::_getAudioRead(
        const OTIO_NS::MediaReference* mediaReference,
        const IOOptions& ioOptions)
    {
        FTK_P();
        return p.getCached<IAudioRead>(
            p.audioReadCache,
            mediaReference,
            ioOptions,
            [](const std::shared_ptr<ftk::Context>& context,
                const ftk::Path& path,
                const std::vector<ftk::MemFile>& mem,
                const IOOptions& options)
            {
                return context->getSystem<ReadSystem>()->audioRead(
                    path, mem, options);
            });
    }

    std::future<VideoData> Timeline::_readVideo(
        const OTIO_NS::Clip* clip,
        const OTIO_NS::RationalTime& time,
        const IOOptions& options)
    {
        FTK_P();
        std::future<VideoData> out;
        IOOptions optionsMerged = merge(options, p.options.ioOptions);
        optionsMerged["USD/CameraName"] = clip->name();
        const auto mediaReference = p.mediaReference(clip);
        // A sequence is decoded on the timeline's pool; anything that has to
        // be read statefully keeps its own reader.
        auto seq = _getSeqDecode(mediaReference, optionsMerged);
        auto read = seq ? nullptr : _getVideoRead(mediaReference, optionsMerged);
        const auto timeRangeOpt = p.getTrimmedRangeInParent(clip);
        if ((seq || read) && timeRangeOpt.has_value())
        {
            const IOInfo& ioInfo = seq ? seq->getInfo() : read->getInfo().get();
            if (!ioInfo.videoTime.has_value())
            {
                // No video in the media, so there is no frame to read and no
                // rate to convert the time with.
                return out;
            }
            OTIO_NS::TimeRange availableRange = clip->available_range();
            OTIO_NS::TimeRange trimmedRange = clip->trimmed_range();
            if (p.options.compat &&
                availableRange.start_time() > ioInfo.videoTime->start_time())
            {
                //! \bug If the available range is greater than the media time,
                //! assume the media time is wrong (e.g., Picchu) and
                //! compensate for it.
                trimmedRange = OTIO_NS::TimeRange(
                    trimmedRange.start_time() - availableRange.start_time(),
                    trimmedRange.duration());
            }
            const auto mediaTime = toVideoMediaTime(
                time,
                timeRangeOpt.value(),
                trimmedRange,
                ioInfo.videoTime->duration().rate());
            out = seq ?
                p.submitRead(
                    [seq, mediaTime, optionsMerged]
                    {
                        return seq->readVideo(mediaTime, optionsMerged);
                    }) :
                read->readVideo(mediaTime, optionsMerged);
        }
        return out;
    }

    std::future<AudioData> Timeline::_readAudio(
        const OTIO_NS::Clip* clip,
        const OTIO_NS::TimeRange& timeRange,
        const IOOptions& options)
    {
        FTK_P();
        std::future<AudioData> out;
        IOOptions optionsMerged = merge(options, p.options.ioOptions);
        auto read = _getAudioRead(clip, optionsMerged);
        const auto timeRangeOpt = p.getTrimmedRangeInParent(clip);
        if (read && timeRangeOpt.has_value())
        {
            const IOInfo& ioInfo = read->getInfo().get();
            OTIO_NS::TimeRange trimmedRange = clip->trimmed_range();
            if (p.options.compat &&
                ioInfo.audioTime.has_value() &&
                trimmedRange.start_time() < ioInfo.audioTime->start_time())
            {
                //! \bug If the trimmed range is less than the media time,
                //! assume the media time is wrong (e.g., ALab trailer) and
                //! compensate for it.
                trimmedRange = OTIO_NS::TimeRange(
                    ioInfo.audioTime->start_time() + trimmedRange.start_time(),
                    trimmedRange.duration());
            }
            const auto mediaRange = toAudioMediaTime(
                timeRange,
                timeRangeOpt.value(),
                trimmedRange,
                ioInfo.audio.sampleRate);
            out = read->readAudio(mediaRange, optionsMerged);
        }
        return out;
    }

    bool Timeline::_getVideoInfo(const OTIO_NS::Composable* composable)
    {
        FTK_P();
        if (auto clip = dynamic_cast<const OTIO_NS::Clip*>(composable))
        {
            if (auto context = p.context.lock())
            {
                // The first video clip defines the video information for the timeline.
                IOInfo ioInfo;
                if (_getVideoIOInfo(
                    p.mediaReference(clip), p.options.ioOptions, ioInfo))
                {
                    p.ioInfo.video = ioInfo.video;
                    p.ioInfo.videoTime = ioInfo.videoTime;
                    p.ioInfo.tags.insert(ioInfo.tags.begin(), ioInfo.tags.end());

                    // Find the largest resolution among the clip's media
                    // references, so that the canvas can hold the highest
                    // resolution one rather than only the reference that is
                    // active now. The readers opened here stay in the read
                    // cache, which also makes the first switch faster.
                    //
                    // The information reported by getIOInfo() is left as that
                    // of the active reference, since that is the media being
                    // played.
                    p.maxVideoSize = !p.ioInfo.video.empty() ?
                        p.ioInfo.video[0].size :
                        ftk::Size2I();
                    p.videoInfoClip = clip;
                    for (const auto& i : clip->media_references())
                    {
                        IOInfo mediaReferenceInfo;
                        if (_getVideoIOInfo(
                            i.second, p.options.ioOptions, mediaReferenceInfo))
                        {
                            // Kept so that getIOInfo() can report the media
                            // that is actually being read; completed with the
                            // timeline level information once it is known.
                            p.videoInfoByReference[i.second] = mediaReferenceInfo;

                            if (!mediaReferenceInfo.video.empty())
                            {
                                const ftk::Size2I& size =
                                    mediaReferenceInfo.video[0].size;
                                if (size.w * size.h >
                                    p.maxVideoSize.w * p.maxVideoSize.h)
                                {
                                    p.maxVideoSize = size;
                                }
                            }
                        }
                    }
                    return true;
                }
            }
        }
        if (auto composition = dynamic_cast<const OTIO_NS::Composition*>(composable))
        {
            for (const auto& child : composition->children())
            {
                if (_getVideoInfo(child))
                {
                    return true;
                }
            }
        }
        return false;
    }

    void Timeline::_getCanvas()
    {
        FTK_P();
        // The OTIO spatial coordinates describe a single canvas shared by the
        // whole timeline, so the extent is taken from every clip rather than
        // from the clips visible at one time. This keeps the render size
        // stable as playback moves between clips.
        // Built from the largest media reference resolution rather than from
        // the reference that is active, so that the canvas does not cap a
        // switch to a higher resolution reference; see _getVideoInfo().
        p.normalizeSize = p.maxVideoSize.isValid() ?
            p.maxVideoSize :
            (!p.ioInfo.video.empty() ? p.ioInfo.video[0].size : ftk::Size2I());
        const ftk::Size2I& normalizeSize = p.normalizeSize;
        const auto otioClips = p.otioTimeline.value->find_children<OTIO_NS::Clip>();

        // The coordinates are unit-less, so a reference is needed to map them
        // onto a pixel size. Take it from the first clip that has coordinates,
        // which is not necessarily the first clip in the timeline, together
        // with the resolution the timeline is working at.
        //
        // This uses the active media reference rather than the union of all of
        // them, unlike the canvas below. The coordinates of a clip's
        // references describe the same area, so any of them gives the same
        // scale; taking the union here would only matter for a clip whose
        // references were authored inconsistently, where the active one is the
        // better guide.
        if (normalizeSize.isValid())
        {
            for (const auto& otioClip : otioClips)
            {
                if (const auto bounds = getClipBounds(otioClip))
                {
                    const float w = bounds.value().size().w;
                    if (w > 0.F)
                    {
                        p.boundsScale = normalizeSize.w / w;
                        break;
                    }
                }
            }
        }

        std::optional<ftk::Box2F> canvas;
        for (const auto& otioClip : otioClips)
        {
            // Report the coordinates as they were authored, so the numbers in
            // the file can be seen alongside the canvas derived from them.
            if (const auto authored = getClipBounds(otioClip))
            {
                p.ioInfo.tags[ftk::Format("OTIO Image Bounds {0}").
                    arg(otioClip->name())] =
                    ftk::Format("{0}, {1}, {2}, {3}").
                    arg(authored.value().min.x).
                    arg(authored.value().min.y).
                    arg(authored.value().max.x).
                    arg(authored.value().max.y);
            }
            // Cover every media reference, not just the active one, so that
            // changing the active media reference cannot place a clip outside
            // the canvas.
            if (const auto bounds = getSpatialBounds(
                getClipBoundsUnion(otioClip),
                p.options.spatial,
                normalizeSize,
                p.boundsScale))
            {
                canvas = canvas.has_value() ?
                    ftk::expand(canvas.value(), bounds.value()) :
                    bounds.value();
            }
        }
        if (canvas.has_value())
        {
            const ftk::Size2F size = canvas.value().size();
            if (size.w > 0.F && size.h > 0.F)
            {
                p.canvasOffset = -canvas.value().min;
                p.canvasSize = ftk::Size2I(
                    static_cast<int>(std::round(size.w)),
                    static_cast<int>(std::round(size.h)));
                p.ioInfo.tags["OTIO Canvas"] =
                    ftk::Format("{0}").arg(p.canvasSize);
                p.ioInfo.tags["OTIO Pixels Per Unit"] =
                    ftk::Format("{0}").arg(p.boundsScale);
            }
        }
    }

    bool Timeline::_getAudioInfo(const OTIO_NS::Composable* composable)
    {
        FTK_P();
        if (auto clip = dynamic_cast<const OTIO_NS::Clip*>(composable))
        {
            if (auto context = p.context.lock())
            {
                // The first audio clip defines the audio information for the timeline.
                IOInfo ioInfo;
                if (_getAudioIOInfo(
                    p.mediaReference(clip), p.options.ioOptions, ioInfo))
                {
                    p.ioInfo.audio = ioInfo.audio;
                    p.ioInfo.audioTime = ioInfo.audioTime;
                    p.ioInfo.tags.insert(ioInfo.tags.begin(), ioInfo.tags.end());
                    return true;
                }
            }
        }
        if (auto composition = dynamic_cast<const OTIO_NS::Composition*>(composable))
        {
            for (const auto& child : composition->children())
            {
                if (_getAudioInfo(child))
                {
                    return true;
                }
            }
        }
        return false;
    }

    float Timeline::_transitionValue(double frame, double in, double out) const
    {
        return (frame - in) / (out - in);
    }

    void Timeline::_tick()
    {
        FTK_P();

        const auto t0 = std::chrono::steady_clock::now();

        _requests();

        // Logging.
        auto t1 = std::chrono::steady_clock::now();
        const std::chrono::duration<float> diff = t1 - p.thread.logTimer;
        if (diff > logInterval)
        {
            p.thread.logTimer = t1;
            if (auto logSystem = p.logSystem.lock())
            {
                size_t videoRequestsSize = 0;
                size_t audioRequestsSize = 0;
                {
                    std::unique_lock<std::mutex> lock(p.mutex.mutex);
                    videoRequestsSize = p.mutex.videoRequests.size();
                    audioRequestsSize = p.mutex.audioRequests.size();
                }
                logSystem->print(
                    ftk::Format("tl::Timeline {0}").arg(this),
                    ftk::Format(
                        "\n"
                        "    * Path: {0}\n"
                        "    * Video requests: {1}, {2} in-progress, {3} max\n"
                        "    * Audio requests: {4}, {5} in-progress, {6} max").
                    arg(p.path.get()).
                    arg(videoRequestsSize).
                    arg(p.thread.videoRequestsInProgress.size()).
                    arg(getVideoRequestMax()).
                    arg(audioRequestsSize).
                    arg(p.thread.audioRequestsInProgress.size()).
                    arg(p.options.audioRequestMax));
            }
            t1 = std::chrono::steady_clock::now();
        }

        // Sleep for a bit, unless the caller is driving this itself and is
        // waiting on the very request that just finished.
        if (p.options.threaded)
        {
            ftk::sleep(timeout, t0, t1);
        }
    }

    void Timeline::_requests()
    {
        FTK_P();

        // Gather requests.
        std::list<std::shared_ptr<Private::PendingVideoRequest> > newVideoRequests;
        std::list<std::shared_ptr<Private::PendingAudioRequest> > newAudioRequests;
        {
            std::unique_lock<std::mutex> lock(p.mutex.mutex);
            p.thread.cv.wait_for(
                lock,
                logInterval,
                [this]
                {
                    return
                        !_p->thread.running ||
                        !_p->mutex.videoRequests.empty() ||
                        !_p->thread.videoRequestsInProgress.empty() ||
                        !_p->mutex.audioRequests.empty() ||
                        !_p->thread.audioRequestsInProgress.empty();
                });
            while (!p.mutex.videoRequests.empty() &&
                (p.thread.videoRequestsInProgress.size() + newVideoRequests.size()) <
                    getVideoRequestMax())
            {
                newVideoRequests.push_back(p.mutex.videoRequests.front());
                p.mutex.videoRequests.pop_front();
            }
            while (!p.mutex.audioRequests.empty() &&
                (p.thread.audioRequestsInProgress.size() + newAudioRequests.size()) < p.options.audioRequestMax)
            {
                newAudioRequests.push_back(p.mutex.audioRequests.front());
                p.mutex.audioRequests.pop_front();
            }
            // Take a copy of the media reference keys so that the rest of the
            // traversal can resolve media references without locking.
            if (p.mutex.mediaReferenceKeysChanged)
            {
                p.thread.mediaReferenceKey = p.mutex.mediaReferenceKey;
                p.thread.clipMediaReferenceKeys = p.mutex.clipMediaReferenceKeys;
                p.mutex.mediaReferenceKeysChanged = false;
            }
        }

        // Traverse the timeline for new video requests.
        for (auto& request : newVideoRequests)
        {
            for (const auto& otioTrack : p.otioTimeline->video_tracks())
            {
                if (otioTrack->enabled())
                {
                    // Only the item covering the requested time can add a
                    // layer, so bisect for it rather than asking every child of
                    // the track.
                    for (const auto& otioChild : p.getTrackChildrenAt(
                        otioTrack, request->time - p.timeRange.start_time()))
                    {
                        if (auto otioItem = dynamic_cast<OTIO_NS::Item*>(otioChild))
                        {
                            const auto requestTime = request->time - p.timeRange.start_time();
                            OTIO_NS::ErrorStatus errorStatus;
                            const auto range = p.getTrimmedRangeInParent(otioItem);
                            if (range.has_value() && range.value().contains(requestTime))
                            {
                                Private::VideoLayerData videoLayerData;
                                try
                                {
                                    if (auto otioClip = dynamic_cast<const OTIO_NS::Clip*>(otioItem))
                                    {
                                        videoLayerData.image = _readVideo(otioClip, requestTime, request->options);
                                        videoLayerData.bounds = getCanvasBox(
                                            getMediaReferenceBounds(p.mediaReference(otioClip)),
                                            p.options.spatial,
                                            p.normalizeSize,
                                            p.boundsScale,
                                            p.canvasOffset);
                                    }
                                    const auto neighbors = otioTrack->neighbors_of(otioItem, &errorStatus);
                                    if (auto otioTransition = dynamic_cast<OTIO_NS::Transition*>(neighbors.second.value))
                                    {
                                        if (requestTime > range.value().end_time_inclusive() - otioTransition->in_offset())
                                        {
                                            videoLayerData.transition = toTransition(otioTransition->transition_type());
                                            videoLayerData.transitionValue = _transitionValue(
                                                requestTime.value(),
                                                range.value().end_time_inclusive().value() - otioTransition->in_offset().value(),
                                                range.value().end_time_inclusive().value() + otioTransition->out_offset().value() + 1.0);
                                            const auto transitionNeighbors = otioTrack->neighbors_of(otioTransition, &errorStatus);
                                            if (const auto otioClipB = dynamic_cast<OTIO_NS::Clip*>(transitionNeighbors.second.value))
                                            {
                                                videoLayerData.imageB = _readVideo(otioClipB, requestTime, request->options);
                                                videoLayerData.boundsB = getCanvasBox(
                                                    getMediaReferenceBounds(p.mediaReference(otioClipB)),
                                                    p.options.spatial,
                                                    p.normalizeSize,
                                                    p.boundsScale,
                                                    p.canvasOffset);
                                            }
                                        }
                                    }
                                    if (auto otioTransition = dynamic_cast<OTIO_NS::Transition*>(neighbors.first.value))
                                    {
                                        if (requestTime < range.value().start_time() + otioTransition->out_offset())
                                        {
                                            std::swap(videoLayerData.image, videoLayerData.imageB);
                                            std::swap(videoLayerData.bounds, videoLayerData.boundsB);
                                            videoLayerData.transition = toTransition(otioTransition->transition_type());
                                            videoLayerData.transitionValue = _transitionValue(
                                                requestTime.value(),
                                                range.value().start_time().value() - otioTransition->in_offset().value() - 1.0,
                                                range.value().start_time().value() + otioTransition->out_offset().value());
                                            const auto transitionNeighbors = otioTrack->neighbors_of(otioTransition, &errorStatus);
                                            if (const auto otioClipB = dynamic_cast<OTIO_NS::Clip*>(transitionNeighbors.first.value))
                                            {
                                                videoLayerData.image = _readVideo(otioClipB, requestTime, request->options);
                                                videoLayerData.bounds = getCanvasBox(
                                                    getMediaReferenceBounds(p.mediaReference(otioClipB)),
                                                    p.options.spatial,
                                                    p.normalizeSize,
                                                    p.boundsScale,
                                                    p.canvasOffset);
                                            }
                                        }
                                    }
                                }
                                catch (const std::exception&)
                                {
                                    //! \todo How should this be handled?
                                }
                                request->layerData.push_back(std::move(videoLayerData));
                            }
                        }
                    }
                }
            }

            p.thread.videoRequestsInProgress.push_back(request);
        }

        // Traverse the timeline for new audio requests.
        for (auto& request : newAudioRequests)
        {
            for (const auto& otioTrack : p.otioTimeline->audio_tracks())
            {
                if (otioTrack->enabled())
                {
                    for (const auto& otioChild : otioTrack->children())
                    {
                        if (auto otioClip = dynamic_cast<OTIO_NS::Clip*>(otioChild.value))
                        {
                            const auto rangeOptional = p.getTrimmedRangeInParent(otioClip);
                            if (rangeOptional.has_value())
                            {
                                const OTIO_NS::TimeRange clipTimeRange(
                                    rangeOptional.value().start_time().rescaled_to(1.0),
                                    rangeOptional.value().duration().rescaled_to(1.0));
                                const double start = request->seconds -
                                    p.timeRange.start_time().rescaled_to(1.0).value();
                                const OTIO_NS::TimeRange requestTimeRange = OTIO_NS::TimeRange(
                                    OTIO_NS::RationalTime(start, 1.0),
                                    OTIO_NS::RationalTime(1.0, 1.0));
                                if (requestTimeRange.intersects(clipTimeRange))
                                {
                                    Private::AudioLayerData audioData;
                                    audioData.seconds = request->seconds;
                                    try
                                    {
                                        //! \bug Why is OTIO_NS::TimeRange::clamped() not giving us the
                                        //! result we expect?
                                        //audioData.timeRange = requestTimeRange.clamped(clipTimeRange);
                                        const double start = std::max(
                                            clipTimeRange.start_time().value(),
                                            requestTimeRange.start_time().value());
                                        const double end = std::min(
                                            clipTimeRange.start_time().value() + clipTimeRange.duration().value(),
                                            requestTimeRange.start_time().value() + requestTimeRange.duration().value());
                                        audioData.timeRange = OTIO_NS::TimeRange(
                                            OTIO_NS::RationalTime(start, 1.0),
                                            OTIO_NS::RationalTime(end - start, 1.0));
                                        audioData.audio = _readAudio(otioClip, audioData.timeRange, request->options);
                                    }
                                    catch (const std::exception&)
                                    {
                                        // Left with no audio, and handled
                                        // below by not contributing a layer.
                                    }
                                    // A clip whose media cannot be read
                                    // contributes nothing, the same as a gap.
                                    // An empty layer is not the same as no
                                    // layer: it reaches the player as a
                                    // stream that never produces a sample,
                                    // and playback is timed by the audio, so
                                    // the clock stops and the video stops
                                    // with it.
                                    if (audioData.audio.valid())
                                    {
                                        request->layerData.push_back(std::move(audioData));
                                    }
                                }
                            }
                        }
                    }
                }
            }
            p.thread.audioRequestsInProgress.push_back(request);
        }

        // Check for finished video requests.
        auto videoRequestIt = p.thread.videoRequestsInProgress.begin();
        while (videoRequestIt != p.thread.videoRequestsInProgress.end())
        {
            bool valid = true;
            for (auto& i : (*videoRequestIt)->layerData)
            {
                if (i.image.valid())
                {
                    valid &= i.image.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
                }
                if (i.imageB.valid())
                {
                    valid &= i.imageB.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
                }
            }
            if (valid)
            {
                const auto frame = p.videoFrame(**videoRequestIt);
                p.updateReadErrors();
                (*videoRequestIt)->promise.set_value(frame);
                videoRequestIt = p.thread.videoRequestsInProgress.erase(videoRequestIt);
                continue;
            }
            ++videoRequestIt;
        }

        // Check for finished audio requests.
        auto audioRequestIt = p.thread.audioRequestsInProgress.begin();
        while (audioRequestIt != p.thread.audioRequestsInProgress.end())
        {
            bool valid = true;
            for (auto& i : (*audioRequestIt)->layerData)
            {
                if (i.audio.valid())
                {
                    valid &= i.audio.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
                }
            }
            if (valid)
            {
                const auto frame = p.audioFrame(**audioRequestIt);
                p.updateReadErrors();
                (*audioRequestIt)->promise.set_value(frame);
                audioRequestIt = p.thread.audioRequestsInProgress.erase(audioRequestIt);
                continue;
            }
            ++audioRequestIt;
        }
    }

    void Timeline::_finishRequests()
    {
        FTK_P();
        {
            std::list<std::shared_ptr<Private::PendingVideoRequest> > videoRequests;
            std::list<std::shared_ptr<Private::PendingAudioRequest> > audioRequests;
            {
                std::unique_lock<std::mutex> lock(p.mutex.mutex);
                p.mutex.stopped = true;
                videoRequests = std::move(p.mutex.videoRequests);
                audioRequests = std::move(p.mutex.audioRequests);
            }
            videoRequests.insert(
                videoRequests.begin(),
                p.thread.videoRequestsInProgress.begin(),
                p.thread.videoRequestsInProgress.end());
            p.thread.videoRequestsInProgress.clear();
            audioRequests.insert(
                audioRequests.begin(),
                p.thread.audioRequestsInProgress.begin(),
                p.thread.audioRequestsInProgress.end());
            p.thread.audioRequestsInProgress.clear();
            for (auto& request : videoRequests)
            {
                const auto frame = p.videoFrame(*request);
                p.updateReadErrors();
                request->promise.set_value(frame);
            }
            for (auto& request : audioRequests)
            {
                const auto frame = p.audioFrame(*request);
                p.updateReadErrors();
                request->promise.set_value(frame);
            }
        }
    }

    void Timeline::Private::startReadPool(size_t threadCount)
    {
        readPool.stopped = false;
        for (size_t i = 0; i < std::max(threadCount, size_t(1)); ++i)
        {
            readPool.threads.push_back(std::thread(
                [this]
                {
                    while (true)
                    {
                        ReadPool::Task task;
                        {
                            std::unique_lock<std::mutex> lock(readPool.mutex);
                            readPool.cv.wait(
                                lock,
                                [this]
                                {
                                    return readPool.stopped || !readPool.tasks.empty();
                                });
                            if (readPool.tasks.empty())
                            {
                                // Stopped and drained.
                                return;
                            }
                            task = std::move(readPool.tasks.front());
                            readPool.tasks.pop_front();
                        }
                        try
                        {
                            task.promise.set_value(task.f());
                        }
                        catch (const std::exception&)
                        {
                            // Passed on rather than delivered empty: the
                            // frame still comes out blank, since videoFrame()
                            // catches this and carries on, but it is counted
                            // and logged instead of going by in silence.
                            task.promise.set_exception(std::current_exception());
                        }
                    }
                }));
        }
    }

    void Timeline::Private::stopReadPool()
    {
        std::list<ReadPool::Task> dropped;
        {
            std::unique_lock<std::mutex> lock(readPool.mutex);
            readPool.stopped = true;
            // Whatever has not started decoding is not going to be looked at,
            // so give the frames back empty rather than making the close wait
            // for a queue of them.
            dropped = std::move(readPool.tasks);
            readPool.tasks.clear();
        }
        for (auto& task : dropped)
        {
            task.promise.set_value(VideoData());
        }
        readPool.cv.notify_all();
        for (auto& thread : readPool.threads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
        readPool.threads.clear();
    }

    std::future<VideoData> Timeline::Private::submitRead(
        std::function<VideoData()> f)
    {
        ReadPool::Task task;
        task.f = std::move(f);
        auto out = task.promise.get_future();
        if (readPool.threads.empty())
        {
            // No pool: the caller is the worker.
            try
            {
                task.promise.set_value(task.f());
            }
            catch (const std::exception&)
            {
                task.promise.set_exception(std::current_exception());
            }
            return out;
        }
        bool queued = false;
        {
            std::unique_lock<std::mutex> lock(readPool.mutex);
            if (!readPool.stopped)
            {
                readPool.tasks.push_back(std::move(task));
                queued = true;
            }
        }
        if (queued)
        {
            readPool.cv.notify_one();
        }
        else
        {
            task.promise.set_value(VideoData());
        }
        return out;
    }

    namespace
    {
        template<typename T>
        void eraseRequest(std::list<std::shared_ptr<T> >& list, const T* request)
        {
            for (auto i = list.begin(); i != list.end(); ++i)
            {
                if (i->get() == request)
                {
                    list.erase(i);
                    return;
                }
            }
        }
    }

    bool Timeline::Private::mediaUnavailable(
        const OTIO_NS::MediaReference* mediaReference)
    {
        std::unique_lock<std::mutex> lock(memFilesMutex);
        return unavailableMediaReferences.find(mediaReference) !=
            unavailableMediaReferences.end();
    }

    void Timeline::Private::abandon(
        const std::shared_ptr<PendingVideoRequest>& request)
    {
        if (auto log = logSystem.lock())
        {
            log->print("tl::Timeline", ftk::Format(
                "Video request {0} did not complete: \"{1}\"").
                arg(request->id).arg(path.get()),
                ftk::LogType::Error);
        }
        {
            std::unique_lock<std::mutex> lock(mutex.mutex);
            eraseRequest(mutex.videoRequests, request.get());
        }
        eraseRequest(thread.videoRequestsInProgress, request.get());
        request->promise.set_value(VideoFrame());
    }

    void Timeline::Private::abandon(
        const std::shared_ptr<PendingAudioRequest>& request)
    {
        if (auto log = logSystem.lock())
        {
            log->print("tl::Timeline", ftk::Format(
                "Audio request {0} did not complete: \"{1}\"").
                arg(request->id).arg(path.get()),
                ftk::LogType::Error);
        }
        {
            std::unique_lock<std::mutex> lock(mutex.mutex);
            eraseRequest(mutex.audioRequests, request.get());
        }
        eraseRequest(thread.audioRequestsInProgress, request.get());
        request->promise.set_value(AudioFrame());
    }

    void Timeline::Private::updateReadErrors()
    {
        size_t count = frameErrorCount;
        std::string error = frameError;
        std::unique_lock<std::mutex> lock(readCacheMutex);
        const auto readErrors = [&count, &error](const auto& reads)
        {
            for (const auto& read : reads)
            {
                if (read)
                {
                    count += read->getErrorCount();
                    if (error.empty())
                    {
                        error = read->getError();
                    }
                }
            }
        };
        readErrors(videoReadCache.getValues());
        readErrors(audioReadCache.getValues());
        readErrorMax = std::max(readErrorMax, count);
        {
            std::unique_lock<std::mutex> lock(mutex.mutex);
            mutex.readErrorCount = readErrorMax;
            if (mutex.readError.empty())
            {
                mutex.readError = error;
            }
        }
    }

    VideoFrame Timeline::Private::videoFrame(PendingVideoRequest& request)
    {
        VideoFrame frame;
        if (!ioInfo.video.empty())
        {
            frame.size = ioInfo.video.front().size;
        }
        frame.canvasSize = canvasSize;
        frame.time = request.time;
        for (auto& i : request.layerData)
        {
            VideoLayer layer;
            try
            {
                if (i.image.valid())
                {
                    const VideoData data = i.image.get();
                    layer.image = data.image;
                    layer.missing = data.missing;
                    layer.heldFrom = data.heldFrom;
                }
                if (i.imageB.valid())
                {
                    layer.imageB = i.imageB.get().image;
                }
            }
            catch (const std::exception& e)
            {
                ++frameErrorCount;
                if (frameError.empty())
                {
                    frameError = e.what();
                }
                if (auto logSystemLocked = logSystem.lock())
                {
                    logSystemLocked->print(
                        "tl::Timeline",
                        e.what(),
                        ftk::LogType::Error);
                }
            }
            layer.bounds = i.bounds;
            layer.boundsB = i.boundsB;
            layer.transition = i.transition;
            layer.transitionValue = i.transitionValue;
            frame.layers.push_back(layer);
        }
        return frame;
    }

    AudioFrame Timeline::Private::audioFrame(PendingAudioRequest& request)
    {
        AudioFrame frame;
        frame.seconds = request.seconds;
        for (auto& i : request.layerData)
        {
            AudioLayer layer;
            try
            {
                if (i.audio.valid())
                {
                    const auto audioData = i.audio.get();
                    if (audioData.audio)
                    {
                        layer.audio = padAudioToOneSecond(audioData.audio, i.seconds, i.timeRange);
                    }
                }
            }
            catch (const std::exception& e)
            {
                ++frameErrorCount;
                if (frameError.empty())
                {
                    frameError = e.what();
                }
                if (auto logSystemLocked = logSystem.lock())
                {
                    logSystemLocked->print(
                        "tl::Timeline",
                        e.what(),
                        ftk::LogType::Error);
                }
            }
            frame.layers.push_back(layer);
        }
        if (frame.layers.empty())
        {
            auto audio = Audio::create(ioInfo.audio, ioInfo.audio.sampleRate);
            audio->zero();
            frame.layers.push_back({ audio });
        }
        return frame;
    }

    std::shared_ptr<Audio> Timeline::Private::padAudioToOneSecond(
        const std::shared_ptr<Audio>& audio,
        double seconds,
        const OTIO_NS::TimeRange& range)
    {
        std::list<std::shared_ptr<Audio> > list;
        const double s = seconds - timeRange.start_time().rescaled_to(1.0).value();
        if (range.start_time().value() > s)
        {
            const OTIO_NS::RationalTime t =
                range.start_time() - OTIO_NS::RationalTime(s, 1.0);
            const OTIO_NS::RationalTime t2 =
                t.rescaled_to(audio->getInfo().sampleRate);
            auto silence = Audio::create(audio->getInfo(), t2.value());
            silence->zero();
            list.push_back(silence);
        }
        list.push_back(audio);
        if (range.end_time_exclusive().value() < s + 1.0)
        {
            const OTIO_NS::RationalTime t =
                OTIO_NS::RationalTime(s + 1.0, 1.0) - range.end_time_exclusive();
            const OTIO_NS::RationalTime t2 =
                t.rescaled_to(audio->getInfo().sampleRate);
            auto silence = Audio::create(audio->getInfo(), t2.value());
            silence->zero();
            list.push_back(silence);
        }
        size_t sampleCount = getSampleCount(list);
        auto out = Audio::create(audio->getInfo(), sampleCount);
        moveAudio(list, out->getData(), sampleCount);
        return out;
    }
}

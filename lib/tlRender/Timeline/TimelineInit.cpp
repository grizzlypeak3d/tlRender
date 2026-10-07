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
#include <ftk/Core/Path.h>

#include <opentimelineio/externalReference.h>
#include <opentimelineio/gap.h>
#include <opentimelineio/imageSequenceReference.h>
#include <opentimelineio/transition.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/fetch.h>
#endif // __EMSCRIPTEN__

#include <algorithm>

namespace tl
{
    //! An absolute form of a file name, or the name unchanged when the
    //! working directory cannot be had.
    std::string absoluteFileName(const std::string& fileName)
    {
        std::filesystem::path out = ftk::toFileSystem(fileName);
        if (!out.is_absolute())
        {
            std::error_code ec;
            const std::filesystem::path abs = std::filesystem::absolute(out, ec);
            if (!ec)
            {
                out = abs;
            }
        }
        return ftk::fromFileSystem(out);
    }

    //! An absolute, normalized form of a media path, used only to compare
    //! paths that name the same file in different ways.
    std::string normalMediaPath(const ftk::Path& path)
    {
        return ftk::fromFileSystem(
            ftk::toFileSystem(absoluteFileName(path.get())).
                lexically_normal());
    }

    namespace
    {
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
                        if (std::filesystem::exists(ftk::toFileSystem(audioPath.get())))
                        {
                            out = audioPath;
                            break;
                        }
                    }
                    // The first match is the one taken: the base names are in
                    // the order they are preferred, so a later one must not
                    // replace what an earlier one found.
                    if (!out.isEmpty())
                    {
                        break;
                    }
                }

                // Or use the first audio file.
                if (out.isEmpty())
                {
                    ftk::DirListOptions listOptions;
                    listOptions.filterExt = imageSeqAudioExts;
                    // toFileSystem because getDir() is UTF-8: the implicit
                    // conversion reads it as the Windows ANSI code page
                    // instead, which throws for bytes that page cannot map --
                    // and throws here, at the call, where dirList's own
                    // try/catch cannot reach it. That is issue #779, where a
                    // Korean file name fails on a Korean code page and is
                    // merely mangled on a Western one.
                    const auto entries = ftk::dirList(
                        ftk::toFileSystem(path.getDir()), listOptions);
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
            audioPath.isEmpty() ?
                path.get() :
                ftk::Format("{0} (audio {1})").
                    arg(path.get()).
                    arg(audioPath.get()).
                    str());

        // A file that is not there, said the same way whatever the file is.
        // Every format arrives here, while the failure itself surfaces much
        // further down and differently in each of them -- from ftk's file
        // I/O for OpenEXR, from a check of its own in OpenImageIO, from a
        // thread in FFmpeg -- so the one thing a person is most likely to
        // hit read five different ways depending on the extension.
        //
        // Not for a sequence, whose path is a pattern rather than a file and
        // whose missing frames are the sequence's own business, nor for a
        // protocol, which is not the filesystem's to answer for. Nor on the
        // web, where a relative path can be a URL that only the fetch can
        // answer for.
#if !defined(__EMSCRIPTEN__)
        if (!path.hasProtocol() &&
            !path.isSeq() &&
            !std::filesystem::exists(
                ftk::toFileSystem(path.getFileName(true))))
        {
            throw std::runtime_error(
                ftk::Format("No such file or directory: \"{0}\"").
                arg(path.get()).str());
        }
#endif // __EMSCRIPTEN__

        OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otioTimeline;

        // Is the input a sequence?
        const std::vector<std::string> seqExts = getExts(
            context,
            static_cast<int>(FileType::Seq));
        const bool hasSeqExt = std::find(
            seqExts.begin(),
            seqExts.end(),
            ftk::toLower(path.getExt())) != seqExts.end();
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
        // What a reader said went wrong, when it said it somewhere other than
        // by throwing.
        std::string readError;
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
            }
            IOInfo audioInfo;
            if (audioFuture.valid())
            {
                audioInfo = audioFuture.get();
            }
            info = merge(videoInfo, audioInfo);

            // Whether anything was read, rather than whether there was a
            // reader to ask. A reader that reports its failure on its own
            // thread -- FFmpeg does -- answers the information request with
            // nothing instead of throwing, and taking that for a readable
            // file would build a timeline with no tracks in it: opening a
            // movie that is not there would give an empty tab rather than
            // an error.
            infoValid = !info.video.empty() || info.audio.isValid();
            if (!infoValid)
            {
                // The reader's own account of it, so that a file that is
                // missing says so rather than being reported as unreadable
                // for reasons unknown.
                const std::string error = videoRead ?
                    videoRead->getError() :
                    std::string();
                readError = !error.empty() ?
                    error :
                    (audioRead ? audioRead->getError() : std::string());
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

                    // Whether it read anything, rather than whether there
                    // was a reader to ask -- the same distinction the merge
                    // above draws, and for the same reason. A reader that
                    // fails on its own thread answers with nothing, leaving
                    // no time range to give the clip.
                    if (audioInfo.audio.isValid() &&
                        audioInfo.audioTime.has_value())
                    {
                        auto audioClip = new OTIO_NS::Clip;
                        audioClip->set_source_range(*audioInfo.audioTime);
                        // Absolute, unlike the video reference above: that one
                        // names the file the timeline was made from and is found
                        // beside it, while the audio was chosen separately and
                        // need not be in the same place. Naming it relatively
                        // would not say the same thing, since a relative
                        // reference is resolved against the timeline's directory
                        // rather than against the working directory the audio was
                        // found from.
                        audioClip->set_media_reference(new OTIO_NS::ExternalReference(
                            absoluteFileName(audioPath.getFileName(true)),
                            audioInfo.audioTime));

                        audioTrack = new OTIO_NS::Track("Audio", std::nullopt, OTIO_NS::Track::Kind::audio);
                        audioTrack->append_child(audioClip);
                    }
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

            // A still image has no duration of its own: paired with audio
            // it lasts as long as the audio, rather than making a timeline
            // one frame long that plays a twenty-fourth of a second of it
            // over and over. The image is read at every frame of the range,
            // which is what a still reference in an OTIO file already does.
            if (videoTrack && audioTrack &&
                1 == videoTrack->children().size() &&
                FileType::Seq == ioSystem->getFileType(ftk::toLower(path.getExt())) &&
                !path.isSeq())
            {
                if (auto clip = dynamic_cast<OTIO_NS::Clip*>(
                    videoTrack->children().front().value))
                {
                    const double rate = info.videoTime->duration().rate();
                    const OTIO_NS::RationalTime duration =
                        audioTrack->duration().rescaled_to(rate).ceil();
                    if (duration > clip->source_range()->duration())
                    {
                        const OTIO_NS::TimeRange range(
                            info.videoTime->start_time(),
                            duration);
                        clip->set_source_range(range);
                        if (auto reference = dynamic_cast<OTIO_NS::ExternalReference*>(
                            clip->media_reference()))
                        {
                            reference->set_available_range(range);
                        }
                    }
                }
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
#if defined(__EMSCRIPTEN__)
                // There is no file system on the web: the timeline is
                // fetched -- a relative path resolves against the
                // page -- and parsed from memory. The fetch can be
                // synchronous because opening runs off the main
                // thread.
                emscripten_fetch_attr_t attr;
                emscripten_fetch_attr_init(&attr);
                strcpy(attr.requestMethod, "GET");
                attr.attributes =
                    EMSCRIPTEN_FETCH_LOAD_TO_MEMORY |
                    EMSCRIPTEN_FETCH_SYNCHRONOUS;
                emscripten_fetch_t* fetch = emscripten_fetch(
                    &attr, fileName.c_str());
                std::string json;
                if (fetch)
                {
                    if (200 == fetch->status)
                    {
                        json = std::string(
                            fetch->data, fetch->numBytes);
                    }
                    emscripten_fetch_close(fetch);
                }
                if (!json.empty())
                {
                    otioTimeline = dynamic_cast<OTIO_NS::Timeline*>(
                        OTIO_NS::Timeline::from_json_string(
                            json, &otioError));
                }
#else // __EMSCRIPTEN__
                otioTimeline = dynamic_cast<OTIO_NS::Timeline*>(
                    OTIO_NS::Timeline::from_json_file(fileName, &otioError));
#endif // __EMSCRIPTEN__
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
            // so say which -- and when the reader said why, say that instead.
            throw std::runtime_error(
                !readError.empty() ?
                readError :
                (ioSystem->getPlugin(path) ?
                ftk::Format("Cannot read the file: \"{0}\"").
                arg(path.get()).str() :
                ftk::Format("Unsupported file format: \"{0}\"").
                arg(path.get()).str()));
        }

        OTIO_NS::AnyDictionary dict;
        dict["path"] = path.get();
        dict["audioPath"] = audioPath.get();
        otioTimeline->metadata()["tlRender"] = dict;

        // Kept as they are rather than left to the metadata to carry back. A
        // sequence's range is the path's own state and not part of its
        // string, so a path that made the round trip would come back naming
        // one frame -- and callers reopen what getPath() gives them.
        p.path = path;
        p.audioPath = audioPath;

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
            static std::atomic<size_t> logIdCounter(0);
            p.logId = ++logIdCounter;
        }
        {
            std::vector<std::string> lines;
            lines.push_back(std::string());
            lines.push_back(ftk::Format("    * Image sequence audio: {0}").
                arg(options.imageSeqAudio));
            lines.push_back(ftk::Format("    * Image sequence audio extensions: {0}").
                arg(ftk::join(options.imageSeqAudioExts, ", ")));
            lines.push_back(ftk::Format("    * Image sequence audio file name: {0}").
                arg(options.imageSeqAudioFileName));
            lines.push_back(ftk::Format("    * Compatibility: {0}").
                arg(options.compat));
            lines.push_back(ftk::Format("    * Read thread count: {0}").
                arg(options.readThreadCount));
            lines.push_back(ftk::Format("    * Audio request max: {0}").
                arg(options.audioRequestMax));
            for (const auto& i : options.ioOptions)
            {
                // Not the USD camera, which is the clip's name and so
                // belongs to one request rather than to the session: it
                // changes with every thumbnail and would make every
                // timeline's options look new.
                if ("USD/CameraName" == i.first)
                {
                    continue;
                }
                lines.push_back(ftk::Format("    * AV I/O {0}: {1}").
                    arg(i.first).
                    arg(i.second));
            }
            lines.push_back(ftk::Format("    * Path max number digits: {0}").
                arg(options.pathOptions.seqMaxDigits));
            // These describe the configuration, not this timeline, and a
            // session opens many timelines with the same ones -- the
            // thumbnails alone make one per request. Printed when they
            // change, which is what a reader needs and is otherwise more
            // than half of the log.
            const std::string text = ftk::join(lines, "\n");
            static std::mutex logOptionsMutex;
            static std::string logOptionsPrev;
            bool changed = false;
            {
                std::unique_lock<std::mutex> lock(logOptionsMutex);
                changed = text != logOptionsPrev;
                if (changed)
                {
                    logOptionsPrev = text;
                }
            }
            if (changed)
            {
                logSystem->print("tl::Timeline options", text);
            }
        }

        p.otioTimeline = otioTimeline;
        if (const auto i = otioTimeline->metadata().find("tlRender");
            i != otioTimeline->metadata().end())
        {
            try
            {
                const auto dict = std::any_cast<OTIO_NS::AnyDictionary>(i->second);
                // Only when the paths were not given directly. Opening a
                // file sets them, and what the metadata holds is a string
                // that cannot say a sequence's range; a timeline handed over
                // as OTIO has nothing better to offer.
                if (auto j = dict.find("path");
                    j != dict.end() && p.path.isEmpty())
                {
                    p.path = ftk::Path(std::any_cast<std::string>(j->second));
                }
                if (auto j = dict.find("audioPath");
                    j != dict.end() && p.audioPath.isEmpty())
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
                        p.path.getProtocol() + p.path.getDir(),
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
            ioInfo.videoSource = i.second.videoSource;
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
            ftk::Format("tl::Timeline {0}").arg(p.logId),
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
                    // Cancel what the readers still hold before the
                    // epilogue: it completes every pending promise, and
                    // each one waits on its reader future -- without the
                    // cancel that is a dead player's cache fill being
                    // decoded to the end. A cancelled request completes
                    // with default values, so the epilogue's waits
                    // return at once; only a frame already in the
                    // decoder finishes. After the loop nothing
                    // dispatches, so no request can slip in behind the
                    // cancel.
                    for (const auto& read : p.videoReadCache.getValues())
                    {
                        read->cancelRequests();
                    }
                    for (const auto& read : p.audioReadCache.getValues())
                    {
                        read->cancelRequests();
                    }
                    _finishRequests();
                });
        }
    }
}

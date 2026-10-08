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

#include <algorithm>

namespace tl
{
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
            path.getProtocol() + path.getDir(),
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
        const IOOptions& options,
        std::string* path)
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
        if (path)
        {
            // The reader's own path rather than one derived again from the
            // media reference, so the layer names exactly what was read.
            if (seq)
            {
                *path = seq->getPath().get();
            }
            else if (read)
            {
                *path = read->getPath().get();
            }
        }
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
                    },
                    time + p.timeRange.start_time()) :
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
                    p.ioInfo.videoSource = ioInfo.videoSource;
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
                    p.ioInfo.audioSource = ioInfo.audioSource;
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

    bool Timeline::Private::mediaUnavailable(
        const OTIO_NS::MediaReference* mediaReference)
    {
        std::unique_lock<std::mutex> lock(memFilesMutex);
        return unavailableMediaReferences.find(mediaReference) !=
            unavailableMediaReferences.end();
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
}

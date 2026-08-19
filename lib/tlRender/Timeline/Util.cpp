// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/Util.h>

#include <tlRender/IO/System.h>

#include <tlRender/Core/URL.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/Error.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/Path.h>
#include <ftk/Core/String.h>

#include <opentimelineio/clip.h>
#include <opentimelineio/externalReference.h>
#include <opentimelineio/imageSequenceReference.h>

#include <ctime>

namespace tl
{
    std::vector<std::string> getExts(
        const std::shared_ptr<ftk::Context>& context,
        int types)
    {
        std::vector<std::string> out;
        if (types & static_cast<int>(FileType::Media))
        {
            out.push_back(".otio");
            out.push_back(".otioz");
        }
        if (auto ioSystem = context->getSystem<ReadSystem>())
        {
            for (const auto& plugin : ioSystem->getPlugins())
            {
                const auto& exts = plugin->getExts(types);
                out.insert(out.end(), exts.begin(), exts.end());
            }
        }
        return out;
    }

    std::vector<OTIO_NS::TimeRange> toRanges(std::vector<OTIO_NS::RationalTime> frames)
    {
        std::vector<OTIO_NS::TimeRange> out;
        if (!frames.empty())
        {
            std::sort(frames.begin(), frames.end());
            auto i = frames.begin();
            auto j = i;
            do
            {
                auto k = j + 1;
                if (k != frames.end() && (*k - *j).value() > 1)
                {
                    out.push_back(OTIO_NS::TimeRange::range_from_start_end_time_inclusive(*i, *j));
                    i = k;
                    j = k;
                }
                else if (k == frames.end())
                {
                    out.push_back(OTIO_NS::TimeRange::range_from_start_end_time_inclusive(*i, *j));
                    i = k;
                    j = k;
                }
                else
                {
                    ++j;
                }
            } while (j != frames.end());
        }
        return out;
    }

    OTIO_NS::RationalTime loop(
        const OTIO_NS::RationalTime& value,
        const OTIO_NS::TimeRange& range,
        bool* looped)
    {
        auto out = value;
        const OTIO_NS::RationalTime start = range.start_time();
        const OTIO_NS::RationalTime end = range.end_time_inclusive();
        const OTIO_NS::RationalTime duration = range.duration();
        if (duration.value() > 0.0)
        {
            while (out < start)
            {
                if (looped)
                {
                    *looped = true;
                }
                out += duration;
            }
            while (out > end)
            {
                if (looped)
                {
                    *looped = true;
                }
                out -= duration;
            }
        }
        return out;
    }

    int64_t loop(
        int64_t value,
        const OTIO_NS::TimeRange& range,
        bool* looped)
    {
        return loop(
            OTIO_NS::RationalTime(value, 1.0),
            OTIO_NS::TimeRange(
                range.start_time().rescaled_to(1.0),
                range.duration().rescaled_to(1.0)),
            looped).value();
    }

    std::vector<OTIO_NS::TimeRange> loop(
        const OTIO_NS::TimeRange& range,
        const OTIO_NS::TimeRange& bounds)
    {
        std::vector<OTIO_NS::TimeRange> out;
        const OTIO_NS::RationalTime& rs = range.start_time();
        const OTIO_NS::RationalTime& bs = bounds.start_time();
        const OTIO_NS::RationalTime re = range.end_time_inclusive();
        const OTIO_NS::RationalTime be = bounds.end_time_inclusive();
        const OTIO_NS::RationalTime one(1.0, range.duration().rate());
        if (rs >= bs && re <= be)
        {
            out.push_back(range);
        }
        else if (rs < bs && re > be)
        {
            out.push_back(bounds);
        }
        else if (rs < bs && re >= bs)
        {
            out.push_back(OTIO_NS::TimeRange::range_from_start_end_time_inclusive(
                be - (bs - rs - one),
                be));
            out.push_back(OTIO_NS::TimeRange::range_from_start_end_time_inclusive(
                bs,
                re));
        }
        else if (rs <= be && re > be)
        {
            out.push_back(OTIO_NS::TimeRange::range_from_start_end_time_inclusive(
                rs,
                be));
            out.push_back(OTIO_NS::TimeRange::range_from_start_end_time_inclusive(
                bs,
                bs + (re - be - one)));
        }
        return out;
    }

    std::vector<ftk::Range<int64_t> > loop(
        const ftk::Range<int64_t>& range,
        const ftk::Range<int64_t>& bounds)
    {
        std::vector<ftk::Range<int64_t> > out;
        const int64_t rs = range.min();
        const int64_t bs = bounds.min();
        const int64_t re = range.max();
        const int64_t be = bounds.max();
        if (rs >= bs && re <= be)
        {
            out.push_back(range);
        }
        else if (rs < bs && re > be)
        {
            out.push_back(bounds);
        }
        else if (rs < bs && re >= bs)
        {
            out.push_back(ftk::Range<int64_t>(
                be - (bs - rs - 1),
                be));
            out.push_back(ftk::Range<int64_t>(
                bs,
                re));
        }
        else if (rs <= be && re > be)
        {
            out.push_back(ftk::Range<int64_t>(
                rs,
                be));
            out.push_back(ftk::Range<int64_t>(
                bs,
                bs + (re - be - 1)));
        }
        return out;
    }

    TL_ENUM_IMPL(
        CacheDir,
        "Forward",
        "Reverse");

    const OTIO_NS::Composable* getRoot(const OTIO_NS::Composable* composable)
    {
        const OTIO_NS::Composable* out = composable;
        for (; out->parent(); out = out->parent())
            ;
        return out;
    }

    std::optional<OTIO_NS::RationalTime> getDuration(
        const OTIO_NS::Timeline* otioTimeline,
        const std::string& kind)
    {
        std::optional<OTIO_NS::RationalTime> out;
        OTIO_NS::ErrorStatus errorStatus;
        for (auto track : otioTimeline->find_children<OTIO_NS::Track>(&errorStatus))
        {
            if (kind == track->kind())
            {
                const OTIO_NS::RationalTime duration = track->duration(&errorStatus);
                if (out.has_value())
                {
                    out = std::max(out.value(), duration);
                }
                else
                {
                    out = duration;
                }
            }
        }
        return out;
    }

    std::optional<OTIO_NS::TimeRange> getTimeRange(const OTIO_NS::Timeline* otioTimeline)
    {
        std::optional<OTIO_NS::TimeRange> out;
        auto duration = getDuration(otioTimeline, OTIO_NS::Track::Kind::video);
        if (!duration.has_value())
        {
            duration = getDuration(otioTimeline, OTIO_NS::Track::Kind::audio);
        }
        if (duration.has_value())
        {
            const OTIO_NS::RationalTime startTime = otioTimeline->global_start_time().has_value() ?
                otioTimeline->global_start_time().value().rescaled_to(duration->rate()) :
                OTIO_NS::RationalTime(0, duration->rate());
            out = OTIO_NS::TimeRange(startTime, duration.value());
        }
        return out;
    }

    std::vector<ftk::Path> getPaths(
        const std::shared_ptr<ftk::Context>& context,
        const ftk::Path& path,
        const ftk::DirListOptions& options)
    {
        std::vector<ftk::Path> out;
        if (std::filesystem::is_directory(std::filesystem::u8path(path.get())))
        {
            auto ioSystem = context->getSystem<ReadSystem>();
            const auto entries = ftk::dirList(path.getFileName(true), options);
            for (const auto& entry : entries)
            {
                const ftk::Path& path = entry.path;
                const std::string ext = ftk::toLower(path.getExt());
                switch (ioSystem->getFileType(ext))
                {
                case FileType::Media:
                case FileType::Seq:
                case FileType::Audio:
                    out.push_back(path);
                    break;
                default:
                    if (".otio" == ext || ".otioz" == ext)
                    {
                        out.push_back(path);
                    }
                    break;
                }
            }
        }
        else if (options.seq && path.testExt(options.seqExts))
        {
            // One frame names the sequence it belongs to, the same way that
            // listing a directory gathers its frames into a single entry --
            // the one flag says both. A path spanning more than one frame is
            // left alone, having said what it covers; a range of one is not,
            // since it cannot be told apart from the frame parsed out of a
            // file name. Whoever means that one frame turns this off.
            ftk::PathOptions pathOptions;
            pathOptions.seqNegative = options.seqNegative;
            pathOptions.seqMaxDigits = options.seqMaxDigits;
            out.push_back(ftk::expandSeq(path, pathOptions));
        }
        else
        {
            out.push_back(path);
        }
        return out;
    }

    ftk::Path getPath(
        const std::string& url,
        const std::string& directory,
        const ftk::PathOptions& pathOptions)
    {
        ftk::Path out(decodeURL(url), pathOptions);
        if (!directory.empty() && !out.hasProtocol() && !out.isAbs())
        {
            out.setDir(ftk::appendSeparator(directory) + out.getDir());
        }
        return out;
    }

    ftk::Path getPath(
        const OTIO_NS::MediaReference* ref,
        const std::string& directory,
        ftk::PathOptions pathOptions)
    {
        std::string url;
        ftk::RangeI64 frames;
        if (auto externalRef = dynamic_cast<const OTIO_NS::ExternalReference*>(ref))
        {
            url = externalRef->target_url();
            pathOptions.seqMaxDigits = 0;
        }
        else if (auto imageSeqRef = dynamic_cast<const OTIO_NS::ImageSequenceReference*>(ref))
        {
            // OTIO adds a separator when the base does not end with one, so do
            // the same. Without this a base written without it gives a path
            // with the directory and the file name run together.
            std::string base = imageSeqRef->target_url_base();
            if (!base.empty() && base.back() != '/' && base.back() != '\\')
            {
                base += '/';
            }
            std::stringstream ss;
            ss << base <<
                imageSeqRef->name_prefix() <<
                std::setfill('0') << std::setw(imageSeqRef->frame_zero_padding()) <<
                imageSeqRef->start_frame() <<
                imageSeqRef->name_suffix();
            url = ss.str();
            frames = ftk::RangeI64(
                imageSeqRef->start_frame(),
                imageSeqRef->end_frame());
        }
        ftk::Path out = getPath(url, directory, pathOptions);
        if (!frames.equal())
        {
            out.setFrames(frames);
        }
        return out;
    }

    MissingFrames fromOTIO(
        OTIO_NS::ImageSequenceReference::MissingFramePolicy value)
    {
        MissingFrames out = MissingFrames::Error;
        switch (value)
        {
        case OTIO_NS::ImageSequenceReference::MissingFramePolicy::hold:
            out = MissingFrames::Hold;
            break;
        case OTIO_NS::ImageSequenceReference::MissingFramePolicy::black:
            out = MissingFrames::Black;
            break;
        default: break;
        }
        return out;
    }

    OTIO_NS::ImageSequenceReference::MissingFramePolicy toOTIO(
        MissingFrames value)
    {
        auto out = OTIO_NS::ImageSequenceReference::MissingFramePolicy::error;
        switch (value)
        {
        case MissingFrames::Hold:
            out = OTIO_NS::ImageSequenceReference::MissingFramePolicy::hold;
            break;
        case MissingFrames::Black:
            out = OTIO_NS::ImageSequenceReference::MissingFramePolicy::black;
            break;
        case MissingFrames::Skip:
        case MissingFrames::Gaps:
            // Nothing to say: the clips over this reference already cover only
            // the frames that are there, so no read reaches a missing one and
            // the policy never comes up. Written as hold so that another
            // application opening the same reference behaves sanely.
            out = OTIO_NS::ImageSequenceReference::MissingFramePolicy::hold;
            break;
        default: break;
        }
        return out;
    }

    OTIO_NS::RationalTime toVideoMediaTime(
        const OTIO_NS::RationalTime& time,
        const OTIO_NS::TimeRange& trimmedRangeInParent,
        const OTIO_NS::TimeRange& trimmedRange,
        double rate)
    {
        OTIO_NS::RationalTime out =
            time - trimmedRangeInParent.start_time() + trimmedRange.start_time();
        out = out.rescaled_to(rate).round();
        return out;
    }

    OTIO_NS::TimeRange toAudioMediaTime(
        const OTIO_NS::TimeRange& timeRange,
        const OTIO_NS::TimeRange& trimmedRangeInParent,
        const OTIO_NS::TimeRange& trimmedRange,
        double sampleRate)
    {
        OTIO_NS::TimeRange out = OTIO_NS::TimeRange(
            timeRange.start_time() - trimmedRangeInParent.start_time() + trimmedRange.start_time(),
            timeRange.duration());
        out = OTIO_NS::TimeRange(
            out.start_time().rescaled_to(sampleRate).round(),
            out.duration().rescaled_to(sampleRate).round());
        return out;
    }

    std::vector<std::shared_ptr<Audio> > audioCopy(
        const AudioInfo& info,
        const std::vector<AudioFrame>& audioFrame,
        Playback playback,
        int64_t frame,
        int64_t size)
    {
        std::vector<std::shared_ptr<Audio> > out;

        // Adjust the frame for reverse playback.
        if (Playback::Reverse == playback)
        {
            frame -= size;
        }

        // Find the first chunk of audio data.
        const int64_t seconds = std::floor(frame / static_cast<double>(info.sampleRate));
        auto secondsIt = std::find_if(
            audioFrame.begin(),
            audioFrame.end(),
            [seconds](const AudioFrame& audioFrame)
            {
                return seconds == audioFrame.seconds;
            });

        // Find the second chunk of audio data.
        const int64_t secondsPlusOne = seconds + 1;
        auto secondsPlusOneIt = std::find_if(
            audioFrame.begin(),
            audioFrame.end(),
            [secondsPlusOne](const AudioFrame& audioFrame)
            {
                return secondsPlusOne == audioFrame.seconds;
            });

        if (secondsIt != audioFrame.end())
        {
            const int64_t offset = frame - seconds * info.sampleRate;

            // Bound the copy by the samples actually available: what remains in
            // the first chunk after `offset`, plus the second chunk if present.
            // This keeps us from reading past either source buffer (a short
            // final second of a clip, or a request larger than two chunks can
            // satisfy at high playback speed) while still advancing the caller's
            // frame counter by exactly the number of samples copied.
            const int64_t firstCount =
                (!secondsIt->layers.empty() && secondsIt->layers[0].audio) ?
                static_cast<int64_t>(secondsIt->layers[0].audio->getSampleCount()) : 0;
            const int64_t avail0 = std::max<int64_t>(0, firstCount - offset);
            const int64_t avail1 =
                (secondsPlusOneIt != audioFrame.end() &&
                    !secondsPlusOneIt->layers.empty() && secondsPlusOneIt->layers[0].audio) ?
                static_cast<int64_t>(secondsPlusOneIt->layers[0].audio->getSampleCount()) : 0;
            const int64_t outSize = std::min(size, avail0 + avail1);

            // Create the output audio.
            for (size_t i = 0; i < secondsIt->layers.size(); ++i)
            {
                auto audio = Audio::create(info, outSize);
                audio->zero();
                out.push_back(audio);
            }

            // Copy audio from the first chunk.
            const int64_t sizeTmp = std::min(outSize, avail0);
            for (size_t i = 0; i < secondsIt->layers.size(); ++i)
            {
                if (secondsIt->layers[i].audio &&
                    secondsIt->layers[i].audio->getInfo() == info)
                {
                    const int64_t n = std::min(
                        sizeTmp,
                        static_cast<int64_t>(secondsIt->layers[i].audio->getSampleCount()) - offset);
                    if (n > 0)
                    {
                        memcpy(
                            out[i]->getData(),
                            secondsIt->layers[i].audio->getData() + offset * info.getByteCount(),
                            n * info.getByteCount());
                    }
                }
            }

            if (sizeTmp < outSize && secondsPlusOneIt != audioFrame.end())
            {
                // Copy audio from the second chunk.
                for (size_t i = 0; i < secondsIt->layers.size() && i < secondsPlusOneIt->layers.size(); ++i)
                {
                    if (secondsPlusOneIt->layers[i].audio &&
                        secondsPlusOneIt->layers[i].audio->getInfo() == info)
                    {
                        const int64_t n = std::min(
                            outSize - sizeTmp,
                            static_cast<int64_t>(secondsPlusOneIt->layers[i].audio->getSampleCount()));
                        if (n > 0)
                        {
                            memcpy(
                                out[i]->getData() + sizeTmp * info.getByteCount(),
                                secondsPlusOneIt->layers[i].audio->getData(),
                                n * info.getByteCount());
                        }
                    }
                }
            }
        }

        return out;
    }
}

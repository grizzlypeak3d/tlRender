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

    std::vector<ftk::MemFile> Timeline::getMem(const OTIO_NS::MediaReference* otioRef)
    {
        FTK_P();
        return *p.getMem(otioRef);
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
        // Opening a file by a relative path would otherwise find none of its
        // own media.
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

    bool Timeline::isMediaTimeContinuous() const
    {
        FTK_P();
        std::optional<std::string> path;
        std::optional<OTIO_NS::RationalTime> end;
        size_t count = 0;
        for (const auto& otioTrack : p.otioTimeline->video_tracks())
        {
            if (!otioTrack->enabled())
            {
                continue;
            }
            for (const auto& otioChild : otioTrack->children())
            {
                auto otioClip = dynamic_cast<const OTIO_NS::Clip*>(otioChild.value);
                if (!otioClip)
                {
                    continue;
                }
                const std::string clipPath = tl::getPath(
                    p.mediaReference(otioClip),
                    p.path.getProtocol() + p.path.getDir(),
                    p.options.pathOptions).get();
                if (path.has_value() && clipPath != path.value())
                {
                    return false;
                }
                path = clipPath;
                const OTIO_NS::TimeRange range = otioClip->trimmed_range();
                if (end.has_value() && range.start_time() < end.value())
                {
                    return false;
                }
                end = range.end_time_exclusive();
                ++count;
            }
        }
        return count > 0;
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
}

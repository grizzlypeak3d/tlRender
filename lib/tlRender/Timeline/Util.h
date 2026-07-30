// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Player.h>

#include <ftk/Core/FileIO.h>

#include <opentimelineio/imageSequenceReference.h>
#include <opentimelineio/mediaReference.h>
#include <opentimelineio/timeline.h>

namespace tl
{
    //! Get the timeline file extensions.
    TL_API std::vector<std::string> getExts(
        const std::shared_ptr<ftk::Context>&,
        int types =
            static_cast<int>(FileType::Media) |
            static_cast<int>(FileType::Seq));

    //! Convert frames to ranges.
    TL_API std::vector<OTIO_NS::TimeRange> toRanges(std::vector<OTIO_NS::RationalTime>);

    //! Loop a time.
    TL_API OTIO_NS::RationalTime loop(
        const OTIO_NS::RationalTime&,
        const OTIO_NS::TimeRange&,
        bool* looped = nullptr);

    //! Loop seconds.
    TL_API int64_t loop(
        int64_t,
        const OTIO_NS::TimeRange&,
        bool* looped = nullptr);

    //! Loop a range.
    TL_API std::vector<OTIO_NS::TimeRange> loop(
        const OTIO_NS::TimeRange&,
        const OTIO_NS::TimeRange&);

    //! Loop a range in seconds.
    TL_API std::vector<ftk::Range<int64_t> > loop(
        const ftk::Range<int64_t>&,
        const ftk::Range<int64_t>&);

    //! Cache direction.
    enum class TL_API_TYPE CacheDir
    {
        Forward,
        Reverse,

        Count,
        First = Forward
    };
    TL_ENUM(CacheDir);

    //! Get the root (highest parent).
    TL_API const OTIO_NS::Composable* getRoot(const OTIO_NS::Composable*);

    //! Get the parent of the given type.
    template<typename T>
    const T* getParent(const OTIO_NS::Item*);

    //! Get the duration of all tracks of the same kind.
    TL_API std::optional<OTIO_NS::RationalTime> getDuration(
        const OTIO_NS::Timeline*,
        const std::string& kind);

    //! Get the time range of a timeline. Unset when no track has a duration.
    TL_API std::optional<OTIO_NS::TimeRange> getTimeRange(const OTIO_NS::Timeline*);

    //! Get a list of paths to open from the given path.
    TL_API std::vector<ftk::Path> getPaths(
        const std::shared_ptr<ftk::Context>&,
        const ftk::Path&,
        const ftk::DirListOptions&);

    //! Get an absolute path.
    TL_API ftk::Path getPath(
        const std::string& url,
        const std::string& directory,
        const ftk::PathOptions&);

    //! Get a path for a media reference.
    TL_API ftk::Path getPath(
        const OTIO_NS::MediaReference*,
        const std::string& directory,
        ftk::PathOptions);

    //! Convert from an OTIO missing frame policy.
    TL_API MissingFrames fromOTIO(
        OTIO_NS::ImageSequenceReference::MissingFramePolicy);

    //! Convert to an OTIO missing frame policy.
    TL_API OTIO_NS::ImageSequenceReference::MissingFramePolicy toOTIO(
        MissingFrames);

    //! Transform track time to video media time.
    TL_API OTIO_NS::RationalTime toVideoMediaTime(
        const OTIO_NS::RationalTime&,
        const OTIO_NS::TimeRange& trimmedRangeInParent,
        const OTIO_NS::TimeRange& trimmedRange,
        double rate);

    //! Transform track time to audio media time.
    TL_API OTIO_NS::TimeRange toAudioMediaTime(
        const OTIO_NS::TimeRange&,
        const OTIO_NS::TimeRange& trimmedRangeInParent,
        const OTIO_NS::TimeRange& trimmedRange,
        double sampleRate);

    //! Copy audio data.
    TL_API std::vector<std::shared_ptr<Audio> > audioCopy(
        const AudioInfo&,
        const std::vector<AudioFrame>&,
        Playback,
        int64_t frame,
        int64_t size);
}

#include <tlRender/Timeline/UtilInline.h>

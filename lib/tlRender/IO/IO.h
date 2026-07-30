// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Core/Audio.h>
#include <tlRender/Core/Time.h>

#include <ftk/Core/Image.h>

namespace tl
{
    //! File types.
    enum class TL_API_TYPE FileType
    {
        Unknown = 0,
        Media = 1,
        Seq = 2,

        Count,
        First = Unknown
    };

    //! I/O information.
    struct TL_API_TYPE IOInfo
    {
        //! Video layer information.
        std::vector<ftk::ImageInfo> video;

        //! Video time range, unset when there is no video.
        std::optional<OTIO_NS::TimeRange> videoTime;

        //! Audio information.
        AudioInfo audio;

        //! Audio time range, unset when there is no audio.
        std::optional<OTIO_NS::TimeRange> audioTime;

        //! Metadata tags.
        ftk::ImageTags tags;

        bool operator == (const IOInfo&) const;
        bool operator != (const IOInfo&) const;
    };

    //! Add the tags describing the video, which are derived from the rest of
    //! the information rather than read from the file.
    TL_API void addVideoTags(IOInfo&);

    //! Merge the video half of the information with the audio half. Video
    //! and audio come from separate readers; this is how the two halves are
    //! put back together.
    TL_API IOInfo merge(const IOInfo& video, const IOInfo& audio);

    //! Video I/O data.
    struct TL_API_TYPE VideoData
    {
        VideoData();
        VideoData(
            const OTIO_NS::RationalTime&,
            uint16_t layer,
            const std::shared_ptr<ftk::Image>&);

        OTIO_NS::RationalTime       time;
        uint16_t                    layer = 0;
        std::shared_ptr<ftk::Image> image;

        bool operator == (const VideoData&) const;
        bool operator != (const VideoData&) const;
        bool operator < (const VideoData&) const;
    };

    //! Audio I/O data.
    struct AudioData
    {
        AudioData();
        AudioData(
            const OTIO_NS::RationalTime&,
            const std::shared_ptr<Audio>&);

        OTIO_NS::RationalTime  time;
        std::shared_ptr<Audio> audio;

        bool operator == (const AudioData&) const;
        bool operator != (const AudioData&) const;
        bool operator < (const AudioData&) const;
    };

    //! Get an integer image type for the given channel count and bit depth.
    TL_API ftk::ImageType getIntImageType(size_t channelCount, size_t bitDepth);

    //! Get a floating point image type for the given channel count and bit
    //! depth.
    TL_API ftk::ImageType getFloatImageType(size_t channelCount, size_t bitDepth);

    //! Options.
    typedef std::map<std::string, std::string> IOOptions;

    //! Merge options.
    TL_API IOOptions merge(const IOOptions&, const IOOptions&);
}

#include <tlRender/IO/IOInline.h>

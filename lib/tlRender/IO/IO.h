// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Core/Audio.h>
#include <tlRender/Core/Time.h>

#include <ftk/Core/Image.h>

#include <optional>

namespace tl
{
    //! File types.
    //! The values are a bit mask; getExts() takes a combination of them.
    enum class TL_API_TYPE FileType
    {
        Unknown = 0,
        Media = 1,
        Seq = 2,
        Audio = 4,

        Count,
        First = Unknown
    };

    //! The video format of the file itself, which is not always the format
    //! it is decoded to: a ProRes file is read as YUV 4:2:2 10 bit and handed
    //! over as something the renderer can take. Empty for the formats that
    //! have nothing to say beyond the decoded information.
    struct TL_API_TYPE VideoSourceInfo
    {
        std::string codec;
        std::string pixelFormat;

        TL_API bool operator == (const VideoSourceInfo&) const;
        TL_API bool operator != (const VideoSourceInfo&) const;
    };

    //! The audio format of the file itself, which is not always the format it
    //! is decoded to.
    struct TL_API_TYPE AudioSourceInfo
    {
        std::string codec;
        AudioType type = AudioType::None;
        size_t channelCount = 0;
        size_t sampleRate = 0;

        TL_API bool operator == (const AudioSourceInfo&) const;
        TL_API bool operator != (const AudioSourceInfo&) const;
    };

    //! I/O information.
    struct TL_API_TYPE IOInfo
    {
        //! Video layer information.
        std::vector<ftk::ImageInfo> video;

        //! Video time range, unset when there is no video.
        std::optional<OTIO_NS::TimeRange> videoTime;

        //! The video format of the file, as opposed to the decoded one above.
        VideoSourceInfo videoSource;

        //! Audio information.
        AudioInfo audio;

        //! Audio time range, unset when there is no audio.
        std::optional<OTIO_NS::TimeRange> audioTime;

        //! The audio format of the file, as opposed to the decoded one above.
        AudioSourceInfo audioSource;

        //! Metadata tags read from the file. Only what the file itself
        //! carries: the information describing the video and the audio is
        //! above, not mixed in here.
        ftk::ImageTags tags;

        TL_API bool operator == (const IOInfo&) const;
        TL_API bool operator != (const IOInfo&) const;
    };

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

        //! Whether the image stands in for a frame the media does not have,
        //! rather than being the frame that was asked for. Reported whatever
        //! the policy, so that showing it is a display choice and costs no
        //! reading.
        bool                        missing = false;

        //! The frame repeated in place of it, when there was one to repeat.
        std::optional<int64_t>      heldFrom;

        TL_API bool operator == (const VideoData&) const;
        TL_API bool operator != (const VideoData&) const;
        TL_API bool operator < (const VideoData&) const;
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

        TL_API bool operator == (const AudioData&) const;
        TL_API bool operator != (const AudioData&) const;
        TL_API bool operator < (const AudioData&) const;
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

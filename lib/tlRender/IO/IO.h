// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/Export.h>
#include <tlRender/Core/Audio.h>
#include <tlRender/Core/Time.h>

#include <ftk/Core/Box.h>
#include <ftk/Core/Image.h>

#include <optional>

namespace tl
{
    //! File types.
    //! The values are a bit mask; getExts() takes a combination of them.
    enum class TL_IO_API_TYPE FileType
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
    struct TL_IO_API_TYPE VideoSourceInfo
    {
        std::string codec;
        std::string pixelFormat;

        bool operator == (const VideoSourceInfo&) const = default;
    };

    //! The audio format of the file itself, which is not always the format it
    //! is decoded to.
    struct TL_IO_API_TYPE AudioSourceInfo
    {
        std::string codec;
        AudioType type = AudioType::None;
        size_t channelCount = 0;
        size_t sampleRate = 0;

        bool operator == (const AudioSourceInfo&) const = default;
    };

    //! I/O information.
    struct TL_IO_API_TYPE IOInfo
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

        TL_IO_API bool operator == (const IOInfo&) const;
        TL_IO_API bool operator != (const IOInfo&) const;
    };

    //! Merge the video half of the information with the audio half. Video
    //! and audio come from separate readers; this is how the two halves are
    //! put back together.
    TL_IO_API IOInfo merge(const IOInfo& video, const IOInfo& audio);

    //! Get the information for a video layer.
    //!
    //! A file's layers each have their own -- an EXR can hold a half colour
    //! layer beside a float depth one -- so reporting a file's video means
    //! saying which layer is being reported. Which one that is comes from the
    //! player.
    //!
    //! The caller has already checked there is a layer to return. An index
    //! past the end is not worth throwing over: it reads as the first layer,
    //! which is what a file with one layer gives anyway.
    TL_IO_API const ftk::ImageInfo& getVideoInfo(const IOInfo&, int layer);

    //! Video I/O data.
    struct TL_IO_API_TYPE VideoData
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

        //! Whether the reader declined the request rather than decoding
        //! it, the playhead having passed the frame while playing (see
        //! IVideoRead::setPlayhead()). The image is empty; the frame is not
        //! what the file holds there, and is not to be kept as such.
        bool                        declined = false;

        //! The frame repeated in place of it, when there was one to repeat.
        std::optional<int64_t>      heldFrom;

        TL_IO_API bool operator == (const VideoData&) const;
        TL_IO_API bool operator != (const VideoData&) const;
        TL_IO_API bool operator < (const VideoData&) const;
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

        TL_IO_API bool operator == (const AudioData&) const;
        TL_IO_API bool operator != (const AudioData&) const;
        TL_IO_API bool operator < (const AudioData&) const;
    };

    //! Get an integer image type for the given channel count and bit depth.
    TL_IO_API ftk::ImageType getIntImageType(size_t channelCount, size_t bitDepth);

    //! Get a floating point image type for the given channel count and bit
    //! depth.
    TL_IO_API ftk::ImageType getFloatImageType(size_t channelCount, size_t bitDepth);

    //! Get the data window and the display window of an image from its
    //! tags, as the OpenEXR reader gives them: "Data Window" and "Display
    //! Window", each the first and last pixel, "x0 y0 x1 y1". In a file of
    //! more than one part they are those of the part the image was read
    //! from: "Part" is its number, and its tags are prefixed "Part N/".
    //! Returns false when the tags have no data window.
    TL_IO_API bool getDataWindow(
        const ftk::ImageTags&,
        ftk::Box2I& dataWindow,
        ftk::Box2I& displayWindow);

    //! Whether a tag describes the file that was read rather than the
    //! picture: the details a reader gives of the file, its streams, and
    //! their codecs, prefixed "File/", "Video/", and "Audio/". They are not
    //! written into another file, which has details of its own.
    TL_IO_API bool isSourceTag(const std::string&);

    //! Options.
    //!
    //! A plugin's own keys are "Plugin/Option"; the bare keys are for any
    //! reader that can honor them:
    //! - "Layer": the image layer to read, as a number.
    //! - "Texture": "1" lets a reader that decodes on the GPU deliver
    //!   frames as textures (ftk::Image::getTextureHandle(), no pixel
    //!   data), for a caller that only draws them. Thumbnails always read
    //!   pixels.
    typedef std::map<std::string, std::string> IOOptions;

    //! Merge options.
    TL_IO_API IOOptions merge(const IOOptions&, const IOOptions&);
}

#include <tlRender/IO/IOInline.h>

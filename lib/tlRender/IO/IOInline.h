// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

namespace tl
{
    inline bool VideoSourceInfo::operator == (const VideoSourceInfo& other) const
    {
        return
            codec == other.codec &&
            pixelFormat == other.pixelFormat;
    }

    inline bool VideoSourceInfo::operator != (const VideoSourceInfo& other) const
    {
        return !(*this == other);
    }

    inline bool AudioSourceInfo::operator == (const AudioSourceInfo& other) const
    {
        return
            codec == other.codec &&
            type == other.type &&
            channelCount == other.channelCount &&
            sampleRate == other.sampleRate;
    }

    inline bool AudioSourceInfo::operator != (const AudioSourceInfo& other) const
    {
        return !(*this == other);
    }

    inline bool IOInfo::operator == (const IOInfo& other) const
    {
        return
            video == other.video &&
            compareExact(videoTime, other.videoTime) &&
            videoSource == other.videoSource &&
            audio == other.audio &&
            compareExact(audioTime, other.audioTime) &&
            audioSource == other.audioSource &&
            tags == other.tags;
    }

    inline bool IOInfo::operator != (const IOInfo& other) const
    {
        return !(*this == other);
    }

    inline VideoData::VideoData()
    {}

    inline VideoData::VideoData(
        const OTIO_NS::RationalTime& time,
        uint16_t layer,
        const std::shared_ptr<ftk::Image>& image) :
        time(time),
        layer(layer),
        image(image)
    {}

    inline bool VideoData::operator == (const VideoData& other) const
    {
        return
            time.strictly_equal(other.time) &&
            layer == other.layer &&
            image == other.image;
    }

    inline bool VideoData::operator != (const VideoData& other) const
    {
        return !(*this == other);
    }

    inline bool VideoData::operator < (const VideoData& other) const
    {
        return time < other.time;
    }

    inline AudioData::AudioData()
    {}

    inline AudioData::AudioData(
        const OTIO_NS::RationalTime& time,
        const std::shared_ptr<Audio>& audio) :
        time(time),
        audio(audio)
    {}

    inline bool AudioData::operator == (const AudioData& other) const
    {
        return
            time.strictly_equal(other.time) &&
            audio == other.audio;
    }

    inline bool AudioData::operator != (const AudioData& other) const
    {
        return !(*this == other);
    }

    inline bool AudioData::operator < (const AudioData& other) const
    {
        return time < other.time;
    }
}
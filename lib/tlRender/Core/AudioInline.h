// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <ftk/Core/Math.h>

#include <cstring>

namespace tl
{
    inline bool AudioInfo::isValid() const
    {
        return
            channelCount > 0 &&
            type != AudioType::None &&
            sampleRate > 0;
    }

    inline size_t AudioInfo::getByteCount() const
    {
        return static_cast<size_t>(channelCount) * tl::getByteCount(type);
    }

    inline const AudioInfo& Audio::getInfo() const
    {
        return _info;
    }

    inline int Audio::getChannelCount() const
    {
        return _info.channelCount;
    }

    inline AudioType Audio::getType() const
    {
        return _info.type;
    }

    inline int Audio::getSampleRate() const
    {
        return _info.sampleRate;
    }

    inline size_t Audio::getSampleCount() const
    {
        return _sampleCount;
    }

    inline bool Audio::isValid() const
    {
        return _info.isValid();
    }

    inline size_t Audio::getByteCount() const
    {
        return _byteCount;
    }

    inline uint8_t* Audio::getData()
    {
        return _data;
    }

    inline const uint8_t* Audio::getData() const
    {
        return _data;
    }
}

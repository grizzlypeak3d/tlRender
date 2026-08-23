// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Core/Audio.h>

#include <ftk/Core/Error.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/String.h>

#include <array>
#include <atomic>
#include <sstream>

namespace tl
{
    FTK_ENUM_IMPL(
        AudioType,
        "None",
        "S8",
        "S16",
        "S32",
        "F32",
        "F64");

    size_t getByteCount(AudioType value)
    {
        const std::array<uint8_t, static_cast<size_t>(AudioType::Count)> data =
        {
            0,
            1,
            2,
            4,
            4,
            8
        };
        return data[static_cast<size_t>(value)];
    }

    AudioType getIntAudioType(int value)
    {
        const std::array<AudioType, 9> data =
        {
            AudioType::None,
            AudioType::S8,
            AudioType::S16,
            AudioType::None,
            AudioType::S32,
            AudioType::None,
            AudioType::None,
            AudioType::None,
            AudioType::None
        };
        return value >= 0 && value < static_cast<int>(data.size()) ? data[value] : AudioType::None;
    }

    AudioType getFloatAudioType(int value)
    {
        const std::array<AudioType, 9> data =
        {
            AudioType::None,
            AudioType::None,
            AudioType::None,
            AudioType::None,
            AudioType::F32,
            AudioType::None,
            AudioType::None,
            AudioType::None,
            AudioType::F64
        };
        return value >= 0 && value < static_cast<int>(data.size()) ? data[value] : AudioType::None;
    }

    AudioInfo::AudioInfo()
    {}

    AudioInfo::AudioInfo(int channelCount, AudioType type, int sampleRate) :
        channelCount(channelCount),
        type(type),
        sampleRate(sampleRate)
    {}

    std::string getLabel(const AudioInfo& info, bool minimal)
    {
        return minimal ?
            ftk::Format("{0}ch {1} {2}kHz").
                arg(info.channelCount).
                arg(info.type).
                arg(info.sampleRate / 1000) :
            ftk::Format("{0} {1} {2} {3}kHz").
                arg(info.channelCount).
                arg(1 == info.channelCount ? "channel" : "channels").
                arg(info.type).
                arg(info.sampleRate / 1000);
    }

    namespace
    {
        std::atomic<size_t> objectCount = 0;
        std::atomic<size_t> totalByteCount = 0;
    }

    Audio::Audio(const AudioInfo& info, size_t sampleCount) :
        _info(info),
        _sampleCount(sampleCount),
        _byteCount(info.getByteCount()* _sampleCount),
        _data(new uint8_t[_byteCount])
    {
        ++objectCount;
        totalByteCount += _byteCount;
    }

    Audio::~Audio()
    {
        delete[] _data;

        --objectCount;
        totalByteCount -= _byteCount;
    }

    std::shared_ptr<Audio> Audio::create(
        const AudioInfo& info,
        size_t sampleCount)
    {
        return std::shared_ptr<Audio>(new Audio(info, sampleCount));
    }

    void Audio::zero()
    {
        std::memset(_data, 0, _byteCount);
    }

    size_t Audio::getObjectCount()
    {
        return objectCount;
    }

    size_t Audio::getTotalByteCount()
    {
        return totalByteCount;
    }

    std::shared_ptr<Audio> combineAudio(const std::list<std::shared_ptr<Audio> >& chunks)
    {
        std::shared_ptr<Audio> out;
        size_t size = 0;
        for (const auto& chunk : chunks)
        {
            size += chunk->getSampleCount();
        }
        if (size > 0)
        {
            out = Audio::create(chunks.front()->getInfo(), size);
            uint8_t* p = out->getData();
            for (const auto& chunk : chunks)
            {
                memcpy(p, chunk->getData(), chunk->getByteCount());
                p += chunk->getByteCount();
            }
        }
        return out;
    }

    namespace
    {
        template<typename T, typename TI>
        void mixI(
            const uint8_t** in,
            size_t inCount,
            uint8_t* out,
            float* volume,
            int channelCount,
            size_t sampleCount)
        {
            const T** inP = reinterpret_cast<const T**>(in);
            T* outP = reinterpret_cast<T*>(out);
            const TI min = static_cast<TI>(std::numeric_limits<T>::min());
            const TI max = static_cast<TI>(std::numeric_limits<T>::max());
            for (size_t i = 0; i < sampleCount; ++i, outP += channelCount)
            {
                for (int j = 0; j < channelCount; ++j)
                {
                    TI v = 0;
                    for (size_t k = 0; k < inCount; ++k)
                    {
                        v += ftk::clamp(static_cast<TI>(inP[k][i * channelCount + j] * volume[j]), min, max);
                    }
                    outP[j] = static_cast<T>(ftk::clamp(v, min, max));
                }
            }
        }

        template<typename T>
        void mixF(
            const uint8_t** in,
            size_t inCount,
            uint8_t* out,
            float* volume,
            int channelCount,
            size_t sampleCount)
        {
            const T** inP = reinterpret_cast<const T**>(in);
            T* outP = reinterpret_cast<T*>(out);
            for (size_t i = 0; i < sampleCount; ++i, outP += channelCount)
            {
                for (int j = 0; j < channelCount; ++j)
                {
                    T v = static_cast<T>(0);
                    for (size_t k = 0; k < inCount; ++k)
                    {
                        v += inP[k][i * channelCount + j] * volume[j];
                    }
                    outP[j] = v;
                }
            }
        }
    }

    std::shared_ptr<Audio> mixAudio(
        const std::vector<std::shared_ptr<Audio> >& in,
        float volume,
        const std::vector<bool>& channelMute)
    {
        std::shared_ptr<Audio> out;
        if (!in.empty())
        {
            const AudioInfo& info = in.front()->getInfo();
            const size_t sampleCount = in.front()->getSampleCount();
            out = Audio::create(info, sampleCount);
            std::vector<const uint8_t*> inP;
            for (size_t i = 0; i < in.size(); ++i)
            {
                inP.push_back(in[i]->getData());
            }
            std::vector<float> channelVolumes;
            for (int i = 0; i < info.channelCount; ++i)
            {
                channelVolumes.push_back(
                    i < static_cast<int>(channelMute.size()) && channelMute[i] ?
                    0.F :
                    volume);
            }
            switch (info.type)
            {
            case AudioType::S8:
                mixI<int8_t, int16_t>(
                    inP.data(),
                    inP.size(),
                    out->getData(),
                    channelVolumes.data(),
                    info.channelCount,
                    sampleCount);
                break;
            case AudioType::S16:
                mixI<int16_t, int32_t>(
                    inP.data(),
                    inP.size(),
                    out->getData(),
                    channelVolumes.data(),
                    info.channelCount,
                    sampleCount);
                break;
            case AudioType::S32:
                mixI<int32_t, int64_t>(
                    inP.data(),
                    inP.size(),
                    out->getData(),
                    channelVolumes.data(),
                    info.channelCount,
                    sampleCount);
                break;
            case AudioType::F32:
                mixF<float>(
                    inP.data(),
                    inP.size(),
                    out->getData(),
                    channelVolumes.data(),
                    info.channelCount,
                    sampleCount);
                break;
            case AudioType::F64:
                mixF<double>(
                    inP.data(),
                    inP.size(),
                    out->getData(),
                    channelVolumes.data(),
                    info.channelCount,
                    sampleCount);
                break;
            default: break;
            }
        }
        return out;
    }

    namespace
    {
        template<typename T>
        void reverseT(
            const uint8_t* in,
            uint8_t*       out,
            size_t         sampleCount,
            int            channelCount)
        {
            const T* inP = reinterpret_cast<const T*>(in) +
                (sampleCount - 1) * channelCount;
            T* outP = reinterpret_cast<T*>(out);
            for (size_t i = 0; i < sampleCount; ++i, inP -= channelCount, outP += channelCount)
            {
                for (int j = 0; j < channelCount; ++j)
                {
                    outP[j] = inP[j];
                }
            }
        }
    }

    std::shared_ptr<Audio> reverseAudio(const std::shared_ptr<Audio>& audio)
    {
        const AudioInfo& info = audio->getInfo();
        const size_t sampleCount = audio->getSampleCount();
        auto out = Audio::create(info, sampleCount);
        switch (info.type)
        {
        case AudioType::S8:
            reverseT<int8_t>(audio->getData(), out->getData(), sampleCount, info.channelCount);
            break;
        case AudioType::S16:
            reverseT<int16_t>(audio->getData(), out->getData(), sampleCount, info.channelCount);
            break;
        case AudioType::S32:
            reverseT<int32_t>(audio->getData(), out->getData(), sampleCount, info.channelCount);
            break;
        case AudioType::F32:
            reverseT<float>(audio->getData(), out->getData(), sampleCount, info.channelCount);
            break;
        case AudioType::F64:
            reverseT<double>(audio->getData(), out->getData(), sampleCount, info.channelCount);
            break;
        default: break;
        }
        return out;
    }

    namespace
    {
        template<typename T>
        void changeSpeedT(
            const uint8_t* in,
            uint8_t*       out,
            size_t         inSampleCount,
            size_t         outSampleCount,
            int            channelCount)
        {
            const T* inP = reinterpret_cast<const T*>(in);
            T* outP = reinterpret_cast<T*>(out);
            for (size_t i = 0; i < outSampleCount; ++i)
            {
                const size_t j = i / static_cast<double>(outSampleCount - 1) *
                    (inSampleCount - 1);
                for (int c = 0; c < channelCount; ++c)
                {
                    outP[i * channelCount + c] = inP[j * channelCount + c];
                }
            }
        }
    }

    std::shared_ptr<Audio> changeAudioSpeed(const std::shared_ptr<Audio>& audio, double mult)
    {
        const AudioInfo& info = audio->getInfo();
        const size_t inSampleCount = audio->getSampleCount();
        const size_t outSampleCount = inSampleCount * mult;
        std::shared_ptr<Audio> out = Audio::create(info, outSampleCount);
        switch (info.type)
        {
        case AudioType::S8:
            changeSpeedT<int8_t>(audio->getData(), out->getData(), inSampleCount, outSampleCount, info.channelCount);
            break;
        case AudioType::S16:
            changeSpeedT<int16_t>(audio->getData(), out->getData(), inSampleCount, outSampleCount, info.channelCount);
            break;
        case AudioType::S32:
            changeSpeedT<int32_t>(audio->getData(), out->getData(), inSampleCount, outSampleCount, info.channelCount);
            break;
        case AudioType::F32:
            changeSpeedT<float>(audio->getData(), out->getData(), inSampleCount, outSampleCount, info.channelCount);
            break;
        case AudioType::F64:
            changeSpeedT<double>(audio->getData(), out->getData(), inSampleCount, outSampleCount, info.channelCount);
            break;
        default: break;
        }
        return out;
    }

    namespace
    {
        typedef int8_t   S8_T;
        typedef int16_t S16_T;
        typedef int32_t S32_T;
        typedef float   F32_T;
        typedef double  F64_T;

        const ftk::Range<S8_T> S8Range(
            std::numeric_limits<S8_T>::min(),
            std::numeric_limits<S8_T>::max());
        const ftk::Range<S16_T> S16Range(
            std::numeric_limits<S16_T>::min(),
            std::numeric_limits<S16_T>::max());
        const ftk::Range<S32_T> S32Range(
            std::numeric_limits<S32_T>::min(),
            std::numeric_limits<S32_T>::max());

        void S8ToS16(S8_T value, S16_T& out)
        {
            out = value * 256;
        }

        void S8ToS32(S8_T value, S32_T& out)
        {
            out = value * 256 * 256 * 256;
        }

        void S8ToF32(S8_T value, F32_T& out)
        {
            out = value / static_cast<float>(S8Range.max());
        }

        void S8ToF64(S8_T value, F64_T& out)
        {
            out = value / static_cast<double>(S8Range.max());
        }

        void S16ToS8(S16_T value, S8_T& out)
        {
            out = value / 256;
        }

        void S16ToS32(S16_T value, S32_T& out)
        {
            out = value * 256 * 256;
        }

        void S16ToF32(S16_T value, F32_T& out)
        {
            out = value / static_cast<float>(S16Range.max());
        }

        void S16ToF64(S16_T value, F64_T& out)
        {
            out = value / static_cast<double>(S16Range.max());
        }

        void S32ToS8(S32_T value, S8_T& out)
        {
            out = value / 256 / 256 / 256;
        }

        void S32ToS16(S32_T value, S16_T& out)
        {
            out = value / 256 / 256;
        }

        void S32ToF32(S32_T value, F32_T& out)
        {
            out = value / static_cast<float>(S32Range.max());
        }

        void S32ToF64(S32_T value, F64_T& out)
        {
            out = value / static_cast<double>(S32Range.max());
        }

        void F32ToS8(F32_T value, S8_T& out)
        {
            out = static_cast<S8_T>(ftk::clamp(
                static_cast<int16_t>(value * S8Range.max()),
                static_cast<int16_t>(S8Range.min()),
                static_cast<int16_t>(S8Range.max())));
        }

        void F32ToS16(F32_T value, S16_T& out)
        {
            out = static_cast<S16_T>(ftk::clamp(
                static_cast<int32_t>(value * S16Range.max()),
                static_cast<int32_t>(S16Range.min()),
                static_cast<int32_t>(S16Range.max())));
        }

        void F32ToS32(F32_T value, S32_T& out)
        {
            out = static_cast<S32_T>(ftk::clamp(
                static_cast<int64_t>(static_cast<int64_t>(value) * S32Range.max()),
                static_cast<int64_t>(S32Range.min()),
                static_cast<int64_t>(S32Range.max())));
        }

        void F32ToF64(F32_T value, F64_T& out)
        {
            out = static_cast<double>(value);
        }

        void F64ToS8(F64_T value, S8_T& out)
        {
            out = static_cast<S8_T>(ftk::clamp(
                static_cast<int16_t>(value * S8Range.max()),
                static_cast<int16_t>(S8Range.min()),
                static_cast<int16_t>(S8Range.max())));
        }

        void F64ToS16(F64_T value, S16_T& out)
        {
            out = static_cast<S16_T>(ftk::clamp(
                static_cast<int32_t>(value * S16Range.max()),
                static_cast<int32_t>(S16Range.min()),
                static_cast<int32_t>(S16Range.max())));
        }

        void F64ToS32(F64_T value, S32_T& out)
        {
            out = static_cast<S32_T>(ftk::clamp(
                static_cast<int64_t>(static_cast<int64_t>(value) * S32Range.max()),
                static_cast<int64_t>(S32Range.min()),
                static_cast<int64_t>(S32Range.max())));
        }

        void F64ToF32(F64_T value, F32_T& out)
        {
            out = static_cast<float>(value);
        }
    }

#define _CONVERT(a, b) \
    { \
        const a##_T * inP = reinterpret_cast<const a##_T *>(in->getData()); \
        b##_T * outP = reinterpret_cast<b##_T *>(out->getData()); \
        for (size_t i = 0; i < sampleCount * channelCount; ++i, ++inP, ++outP) \
        { \
            a##To##b(*inP, *outP); \
        } \
    }

    std::shared_ptr<Audio> convertAudio(const std::shared_ptr<Audio>& in, AudioType type)
    {
        const AudioType inType = in->getType();
        const size_t sampleCount = in->getSampleCount();
        const int channelCount = in->getChannelCount();
        auto out = Audio::create(AudioInfo(channelCount, type, in->getSampleRate()), sampleCount);
        if (inType == type)
        {
            std::memcpy(
                out->getData(),
                in->getData(),
                sampleCount * channelCount * getByteCount(type));
        }
        else
        {
            switch (inType)
            {
            case AudioType::S8:
                switch (type)
                {
                case AudioType::S16: _CONVERT(S8, S16); break;
                case AudioType::S32: _CONVERT(S8, S32); break;
                case AudioType::F32: _CONVERT(S8, F32); break;
                case AudioType::F64: _CONVERT(S8, F64); break;
                default: break;
                }
                break;
            case AudioType::S16:
                switch (type)
                {
                case AudioType::S8:  _CONVERT(S16, S8);  break;
                case AudioType::S32: _CONVERT(S16, S32); break;
                case AudioType::F32: _CONVERT(S16, F32); break;
                case AudioType::F64: _CONVERT(S16, F64); break;
                default: break;
                }
                break;
            case AudioType::S32:
                switch (type)
                {
                case AudioType::S8:  _CONVERT(S32, S8);  break;
                case AudioType::S16: _CONVERT(S32, S16); break;
                case AudioType::F32: _CONVERT(S32, F32); break;
                case AudioType::F64: _CONVERT(S32, F64); break;
                default: break;
                }
                break;
            case AudioType::F32:
                switch (type)
                {
                case AudioType::S8:  _CONVERT(F32, S8);  break;
                case AudioType::S16: _CONVERT(F32, S16); break;
                case AudioType::S32: _CONVERT(F32, S32); break;
                case AudioType::F64: _CONVERT(F32, F64); break;
                default: break;
                }
                break;
            case AudioType::F64:
                switch (type)
                {
                case AudioType::S8:  _CONVERT(F64, S8);  break;
                case AudioType::S16: _CONVERT(F64, S16); break;
                case AudioType::S32: _CONVERT(F64, S32); break;
                case AudioType::F32: _CONVERT(F64, F32); break;
                default: break;
                }
                break;
            default: break;
            }
        }
        return out;
    }

    size_t getSampleCount(const std::list<std::shared_ptr<Audio> >& value)
    {
        size_t out = 0;
        for (const auto& i : value)
        {
            if (i)
            {
                out += i->getSampleCount();
            }
        }
        return out;
    }

    void moveAudio(std::list<std::shared_ptr<Audio> >& in, uint8_t* out, size_t sampleCount)
    {
        size_t size = 0;
        while (!in.empty() && (size + in.front()->getSampleCount() <= sampleCount))
        {
            std::memcpy(out, in.front()->getData(), in.front()->getByteCount());
            size += in.front()->getSampleCount();
            out += in.front()->getByteCount();
            in.pop_front();
        }
        if (!in.empty() && size < sampleCount)
        {
            auto item = in.front();
            in.pop_front();
            const size_t remainingSize = sampleCount - size;
            std::memcpy(
                out,
                item->getData(),
                remainingSize * item->getInfo().getByteCount());
            const size_t newItemSampleCount = item->getSampleCount() - remainingSize;
            auto newItem = Audio::create(item->getInfo(), newItemSampleCount);
            std::memcpy(
                newItem->getData(),
                item->getData() + remainingSize * item->getInfo().getByteCount(),
                newItem->getByteCount());
            in.push_front(newItem);
            size += remainingSize;
        }
    }
}

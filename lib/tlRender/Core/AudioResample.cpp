// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Core/AudioResample.h>

#include <algorithm>
#include <cstring>
#include <vector>

#if defined(TLRENDER_FFMPEG)
extern "C"
{
#include <libswresample/swresample.h>
}
#endif // TLRENDER_FFMPEG

namespace tl
{
    namespace
    {
#if defined(TLRENDER_FFMPEG)
        AVSampleFormat fromAudioType(AudioType value)
        {
            AVSampleFormat out = AV_SAMPLE_FMT_NONE;
            switch (value)
            {
            case AudioType::S16: out = AV_SAMPLE_FMT_S16; break;
            case AudioType::S32: out = AV_SAMPLE_FMT_S32; break;
            case AudioType::F32: out = AV_SAMPLE_FMT_FLT; break;
            case AudioType::F64: out = AV_SAMPLE_FMT_DBL; break;
            default: break;
            }
            return out;
        }
#endif // TLRENDER_FFMPEG
    }

    struct AudioResample::Private
    {
        AudioInfo inputInfo;
        AudioInfo outputInfo;
#if defined(TLRENDER_FFMPEG)
        SwrContext* swrContext = nullptr;
        std::vector<uint8_t> flushBuffer;
#endif // TLRENDER_FFMPEG
    };

    void AudioResample::_init(
        const AudioInfo& inputInfo,
        const AudioInfo& outputInfo)
    {
        FTK_P();
        p.inputInfo = inputInfo;
        p.outputInfo = outputInfo;
#if defined(TLRENDER_FFMPEG)
        if (p.inputInfo.isValid() && p.outputInfo.isValid())
        {
            AVChannelLayout inputChannelLayout;
            av_channel_layout_default(&inputChannelLayout, p.inputInfo.channelCount);
            AVChannelLayout outputChannelLayout;
            av_channel_layout_default(&outputChannelLayout, p.outputInfo.channelCount);
            swr_alloc_set_opts2(
                &p.swrContext,
                &outputChannelLayout,
                fromAudioType(p.outputInfo.type),
                p.outputInfo.sampleRate,
                &inputChannelLayout,
                fromAudioType(p.inputInfo.type),
                p.inputInfo.sampleRate,
                0,
                nullptr);
            av_channel_layout_uninit(&inputChannelLayout);
            av_channel_layout_uninit(&outputChannelLayout);
            if (p.swrContext)
            {
                swr_init(p.swrContext);
            }
        }
#endif // TLRENDER_FFMPEG
    }

    AudioResample::AudioResample() :
        _p(new Private())
    {
    }

    AudioResample::~AudioResample()
    {
        FTK_P();
#if defined(TLRENDER_FFMPEG)
        if (p.swrContext)
        {
            swr_free(&p.swrContext);
        }
#endif // TLRENDER_FFMPEG
    }

    std::shared_ptr<AudioResample> AudioResample::create(
        const AudioInfo& inputInfo,
        const AudioInfo& outputInfo)
    {
        auto out = std::shared_ptr<AudioResample>(new AudioResample);
        out->_init(inputInfo, outputInfo);
        return out;
    }

    const AudioInfo& AudioResample::getInputInfo() const
    {
        return _p->inputInfo;
    }

    const AudioInfo& AudioResample::getOutputInfo() const
    {
        return _p->outputInfo;
    }

    std::shared_ptr<Audio> AudioResample::process(const std::shared_ptr<Audio>& value)
    {
        FTK_P();
        std::shared_ptr<Audio> out;
#if defined(TLRENDER_FFMPEG)
        if (p.swrContext && value)
        {
            const size_t sampleCount = value->getSampleCount();
            auto swrOutputBuffer = Audio::create(p.outputInfo, getOutputSampleCountMax(sampleCount));
            const size_t swrOutputCount = process(
                value->getData(),
                sampleCount,
                swrOutputBuffer->getData(),
                swrOutputBuffer->getSampleCount());
            out = Audio::create(p.outputInfo, swrOutputCount);
            memcpy(out->getData(), swrOutputBuffer->getData(), out->getByteCount());
        }
#endif // TLRENDER_FFMPEG
        return out;
    }

    size_t AudioResample::getOutputSampleCountMax(size_t sampleCount) const
    {
        size_t out = 0;
#if defined(TLRENDER_FFMPEG)
        if (_p->swrContext)
        {
            out = std::max(swr_get_out_samples(_p->swrContext, sampleCount), 0);
        }
#endif // TLRENDER_FFMPEG
        return out;
    }

    size_t AudioResample::process(
        const uint8_t* in,
        size_t sampleCount,
        uint8_t* out,
        size_t outSampleCountMax)
    {
        size_t outCount = 0;
#if defined(TLRENDER_FFMPEG)
        FTK_P();
        if (p.swrContext)
        {
            uint8_t* swrOutputBufferP[] = { out };
            const uint8_t* swrInputBufferP[] = { in };
            const int swrOutputCount = swr_convert(
                p.swrContext,
                swrOutputBufferP,
                outSampleCountMax,
                swrInputBufferP,
                sampleCount);
            outCount = swrOutputCount > 0 ? swrOutputCount : 0;
        }
#endif // TLRENDER_FFMPEG
        return outCount;
    }

    void AudioResample::flush()
    {
        FTK_P();
#if defined(TLRENDER_FFMPEG)
        if (p.swrContext)
        {
            // Into a buffer kept from one flush to the next, which the audio
            // thread calls this from: allocating there can wait on the
            // memory map.
            const int drain = std::max(swr_get_out_samples(p.swrContext, 0), 0);
            const size_t size = drain * p.outputInfo.getByteCount();
            if (p.flushBuffer.size() < size)
            {
                p.flushBuffer.resize(size);
            }
            uint8_t* tmpP[] = { p.flushBuffer.data() };
            swr_convert(
                p.swrContext,
                tmpP,
                drain,
                nullptr,
                0);
        }
#endif // TLRENDER_FFMPEG
    }
}
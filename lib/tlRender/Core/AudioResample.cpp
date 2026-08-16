// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Core/AudioResample.h>

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
            const int swrOutputSamples = swr_get_out_samples(p.swrContext, sampleCount);
            auto swrOutputBuffer = Audio::create(p.outputInfo, swrOutputSamples);
            uint8_t* swrOutputBufferP[] = { swrOutputBuffer->getData() };
            const uint8_t* swrInputBufferP[] = { value->getData() };
            const int swrOutputCount = swr_convert(
                p.swrContext,
                swrOutputBufferP,
                swrOutputSamples,
                swrInputBufferP,
                sampleCount);
            out = Audio::create(p.outputInfo, swrOutputCount > 0 ? swrOutputCount : 0);
            memcpy(out->getData(), swrOutputBuffer->getData(), out->getByteCount());
        }
#endif // TLRENDER_FFMPEG
        return out;
    }

    void AudioResample::flush()
    {
        FTK_P();
#if defined(TLRENDER_FFMPEG)
        if (p.swrContext)
        {
            const int drain = swr_get_out_samples(p.swrContext, 0);
            std::vector<uint8_t> tmp(drain * p.outputInfo.getByteCount(), 0);
            uint8_t* tmpP[] = { tmp.data() };
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
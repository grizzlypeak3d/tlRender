// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/FFmpegPrivate.h>

#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>

#include <algorithm>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libswresample/swresample.h>
}

namespace tl
{
    namespace ffmpeg
    {
        struct Write::Private
        {
            std::string fileName;
            AVFormatContext* avFormatContext = nullptr;
            AVCodecContext* avCodecContext = nullptr;
            AVStream* avVideoStream = nullptr;
            AVPacket* avPacket = nullptr;
            AVFrame* avFrame = nullptr;
            AVPixelFormat avPixelFormatIn = AV_PIX_FMT_NONE;
            AVFrame* avFrame2 = nullptr;
            SwsContext* swsContext = nullptr;

            AVCodecContext* avAudioCodecContext = nullptr;
            AVStream* avAudioStream = nullptr;
            AVPacket* avAudioPacket = nullptr;
            AVFrame* avAudioFrame = nullptr;
            AVSampleFormat avSampleFormatIn = AV_SAMPLE_FMT_NONE;
            SwrContext* swrContext = nullptr;
            AVAudioFifo* avAudioFifo = nullptr;
            int64_t audioSampleCount = 0;

            bool opened = false;
            bool finished = false;
        };

        void Write::_init(
            const ftk::Path& path,
            const IOInfo& info,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            IWrite::_init(path, options, info, logSystem);

            FTK_P();

            p.fileName = path.get();
            if (info.video.empty() || !info.videoTime.has_value())
            {
                throw std::runtime_error(ftk::Format("No video: \"{0}\"").arg(p.fileName));
            }

            int avProfile = AV_PROFILE_UNKNOWN;
            auto option = options.find("FFmpeg/Codec");
            std::string codec;
            if (option != options.end())
            {
                codec = option->second;
            }

            int r = avformat_alloc_output_context2(&p.avFormatContext, NULL, NULL, p.fileName.c_str());
            if (r < 0)
            {
                throw std::runtime_error(ftk::Format("{0}: \"{1}\"").arg(getErrorLabel(r)).arg(p.fileName));
            }
            const AVCodec* avCodec = avcodec_find_encoder_by_name(codec.c_str());
            if (!avCodec)
            {
                throw std::runtime_error(ftk::Format("Cannot find encoder: \"{0}\"").arg(p.fileName));
            }
            p.avCodecContext = avcodec_alloc_context3(avCodec);
            if (!p.avCodecContext)
            {
                throw std::runtime_error(ftk::Format("Cannot allocate context: \"{0}\"").arg(p.fileName));
            }
            p.avVideoStream = avformat_new_stream(p.avFormatContext, avCodec);
            if (!p.avVideoStream)
            {
                throw std::runtime_error(ftk::Format("Cannot allocate stream: \"{0}\"").arg(p.fileName));
            }
            if (!avCodec->pix_fmts)
            {
                throw std::runtime_error(ftk::Format("No pixel formats available: \"{0}\"").arg(p.fileName));
            }

            p.avCodecContext->codec_id = avCodec->id;
            p.avCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
            const auto& videoInfo = info.video[0];
            p.avCodecContext->width = videoInfo.size.w;
            p.avCodecContext->height = videoInfo.size.h;
            p.avCodecContext->sample_aspect_ratio = AVRational({ 1, 1 });
            p.avCodecContext->pix_fmt = avCodec->pix_fmts[0];
            const auto rational = toRational(info.videoTime->duration().rate());
            p.avCodecContext->time_base = { rational.second, rational.first };
            p.avCodecContext->framerate = { rational.first, rational.second };
            p.avCodecContext->profile = avProfile;
            if (p.avFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
            {
                p.avCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }
            p.avCodecContext->thread_count = 0;
            p.avCodecContext->thread_type = FF_THREAD_FRAME;

            // The stream's color description, when the caller says what
            // the pixels are; the names are the ones FFmpeg itself uses,
            // the same as the reader produces. Set on the context before
            // it is opened so the muxer writes it; e.g., the "colr" atom.
            if (const auto i = info.tags.find("Color Primaries");
                i != info.tags.end())
            {
                const int v = av_color_primaries_from_name(i->second.c_str());
                if (v >= 0)
                {
                    p.avCodecContext->color_primaries = static_cast<AVColorPrimaries>(v);
                }
            }
            if (const auto i = info.tags.find("Color Transfer");
                i != info.tags.end())
            {
                const int v = av_color_transfer_from_name(i->second.c_str());
                if (v >= 0)
                {
                    p.avCodecContext->color_trc = static_cast<AVColorTransferCharacteristic>(v);
                }
            }
            if (const auto i = info.tags.find("Color Matrix");
                i != info.tags.end())
            {
                const int v = av_color_space_from_name(i->second.c_str());
                if (v >= 0)
                {
                    p.avCodecContext->colorspace = static_cast<AVColorSpace>(v);
                }
            }
            // The QuickTime muxer only writes a color description that is
            // complete -- primaries, transfer, and matrix together -- so a
            // caller that said what the pixels are but not the matrix would
            // get nothing written at all. The conversion below runs the
            // software scaler with its default BT.601 coefficients, so that
            // is the matrix the file truthfully gets.
            if (p.avCodecContext->color_primaries != AVCOL_PRI_UNSPECIFIED &&
                p.avCodecContext->color_trc != AVCOL_TRC_UNSPECIFIED &&
                AVCOL_SPC_UNSPECIFIED == p.avCodecContext->colorspace)
            {
                const AVPixFmtDescriptor* avPixFmtDesc =
                    av_pix_fmt_desc_get(p.avCodecContext->pix_fmt);
                if (avPixFmtDesc &&
                    !(avPixFmtDesc->flags & AV_PIX_FMT_FLAG_RGB))
                {
                    p.avCodecContext->colorspace = AVCOL_SPC_BT470BG;
                }
            }

            r = avcodec_open2(p.avCodecContext, avCodec, NULL);
            if (r < 0)
            {
                throw std::runtime_error(ftk::Format("{0}: \"{1}\"").arg(getErrorLabel(r)).arg(p.fileName));
            }

            r = avcodec_parameters_from_context(p.avVideoStream->codecpar, p.avCodecContext);
            if (r < 0)
            {
                throw std::runtime_error(ftk::Format("{0}: \"{1}\"").arg(getErrorLabel(r)).arg(p.fileName));
            }

            p.avVideoStream->time_base = { rational.second, rational.first };
            p.avVideoStream->avg_frame_rate = { rational.first, rational.second };

            // Setup the audio stream.
            if (info.audio.isValid())
            {
                std::string audioCodec;
                option = options.find("FFmpeg/AudioCodec");
                if (option != options.end())
                {
                    audioCodec = option->second;
                }
                const AVCodec* avAudioCodec = nullptr;
                if (!audioCodec.empty())
                {
                    avAudioCodec = avcodec_find_encoder_by_name(audioCodec.c_str());
                    if (!avAudioCodec)
                    {
                        throw std::runtime_error(ftk::Format("Cannot find audio encoder \"{0}\": \"{1}\"").
                            arg(audioCodec).arg(p.fileName));
                    }
                }
                const bool containerHasAudio =
                    p.avFormatContext->oformat->audio_codec != AV_CODEC_ID_NONE;
                if (audioCodec.empty() && containerHasAudio)
                {
                    avAudioCodec = avcodec_find_encoder(p.avFormatContext->oformat->audio_codec);
                }
                if (!avAudioCodec)
                {
                    // Two different reasons to write no audio, worth telling
                    // apart: a container that has no place to put it, and a
                    // build without the encoder the container asks for. The
                    // second is what a minimal FFmpeg gives, where the codec
                    // a movie defaults to is left out over licensing, and it
                    // reads as a broken file rather than a missing feature.
                    std::string message =
                        ftk::Format("The output format does not support audio, "
                            "audio will not be written: \"{0}\"").
                        arg(p.fileName);
                    if (containerHasAudio)
                    {
                        message =
                            ftk::Format("This build has no encoder for the audio "
                                "codec \"{0}\" the output format uses, audio will "
                                "not be written: \"{1}\"").
                            arg(avcodec_get_name(
                                p.avFormatContext->oformat->audio_codec)).
                            arg(p.fileName);
                    }
                    if (logSystem)
                    {
                        logSystem->print(
                            "tl::ffmpeg::Write",
                            message,
                            ftk::LogType::Warning);
                    }
                }
                else
                {
                    p.avSampleFormatIn = fromAudioType(info.audio.type);
                    if (AV_SAMPLE_FMT_NONE == p.avSampleFormatIn)
                    {
                        throw std::runtime_error(ftk::Format("Incompatible audio type: \"{0}\"").arg(p.fileName));
                    }

                    p.avAudioCodecContext = avcodec_alloc_context3(avAudioCodec);
                    if (!p.avAudioCodecContext)
                    {
                        throw std::runtime_error(ftk::Format("Cannot allocate context: \"{0}\"").arg(p.fileName));
                    }
                    p.avAudioStream = avformat_new_stream(p.avFormatContext, avAudioCodec);
                    if (!p.avAudioStream)
                    {
                        throw std::runtime_error(ftk::Format("Cannot allocate stream: \"{0}\"").arg(p.fileName));
                    }

                    // Pick a sample format supported by the encoder,
                    // preferring an exact match with the input.
                    AVSampleFormat avSampleFormat = p.avSampleFormatIn;
                    if (avAudioCodec->sample_fmts)
                    {
                        avSampleFormat = avAudioCodec->sample_fmts[0];
                        for (const AVSampleFormat* i = avAudioCodec->sample_fmts;
                            *i != AV_SAMPLE_FMT_NONE;
                            ++i)
                        {
                            if (*i == p.avSampleFormatIn)
                            {
                                avSampleFormat = *i;
                                break;
                            }
                        }
                    }

                    // Pick a sample rate supported by the encoder,
                    // preferring the closest to the input.
                    int sampleRate = info.audio.sampleRate;
                    if (avAudioCodec->supported_samplerates)
                    {
                        int closest = avAudioCodec->supported_samplerates[0];
                        for (const int* i = avAudioCodec->supported_samplerates; *i; ++i)
                        {
                            if (std::abs(*i - sampleRate) < std::abs(closest - sampleRate))
                            {
                                closest = *i;
                            }
                        }
                        sampleRate = closest;
                    }

                    p.avAudioCodecContext->codec_id = avAudioCodec->id;
                    p.avAudioCodecContext->codec_type = AVMEDIA_TYPE_AUDIO;
                    p.avAudioCodecContext->sample_fmt = avSampleFormat;
                    p.avAudioCodecContext->sample_rate = sampleRate;
                    p.avAudioCodecContext->time_base = { 1, sampleRate };
                    av_channel_layout_default(
                        &p.avAudioCodecContext->ch_layout,
                        info.audio.channelCount);
                    if (p.avFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
                    {
                        p.avAudioCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
                    }

                    r = avcodec_open2(p.avAudioCodecContext, avAudioCodec, NULL);
                    if (r < 0)
                    {
                        throw std::runtime_error(ftk::Format("{0}: \"{1}\"").arg(getErrorLabel(r)).arg(p.fileName));
                    }
                    r = avcodec_parameters_from_context(
                        p.avAudioStream->codecpar,
                        p.avAudioCodecContext);
                    if (r < 0)
                    {
                        throw std::runtime_error(ftk::Format("{0}: \"{1}\"").arg(getErrorLabel(r)).arg(p.fileName));
                    }
                    p.avAudioStream->time_base = { 1, sampleRate };

                    // Setup the resampler for converting the input audio
                    // to the encoder format.
                    AVChannelLayout channelLayoutIn;
                    av_channel_layout_default(&channelLayoutIn, info.audio.channelCount);
                    r = swr_alloc_set_opts2(
                        &p.swrContext,
                        &p.avAudioCodecContext->ch_layout,
                        p.avAudioCodecContext->sample_fmt,
                        p.avAudioCodecContext->sample_rate,
                        &channelLayoutIn,
                        p.avSampleFormatIn,
                        info.audio.sampleRate,
                        0,
                        NULL);
                    av_channel_layout_uninit(&channelLayoutIn);
                    if (r < 0 || !p.swrContext)
                    {
                        throw std::runtime_error(ftk::Format("Cannot get context: \"{0}\"").arg(p.fileName));
                    }
                    swr_init(p.swrContext);

                    // The FIFO re-chunks incoming audio into the frame
                    // size required by the encoder.
                    p.avAudioFifo = av_audio_fifo_alloc(
                        p.avAudioCodecContext->sample_fmt,
                        p.avAudioCodecContext->ch_layout.nb_channels,
                        p.avAudioCodecContext->sample_rate);
                    if (!p.avAudioFifo)
                    {
                        throw std::runtime_error(ftk::Format("Cannot allocate FIFO: \"{0}\"").arg(p.fileName));
                    }

                    p.avAudioPacket = av_packet_alloc();
                    if (!p.avAudioPacket)
                    {
                        throw std::runtime_error(ftk::Format("Cannot allocate packet: \"{0}\"").arg(p.fileName));
                    }

                    const int frameSize = p.avAudioCodecContext->frame_size > 0 ?
                        p.avAudioCodecContext->frame_size :
                        1024;
                    p.avAudioFrame = av_frame_alloc();
                    if (!p.avAudioFrame)
                    {
                        throw std::runtime_error(ftk::Format("Cannot allocate frame: \"{0}\"").arg(p.fileName));
                    }
                    p.avAudioFrame->format = p.avAudioCodecContext->sample_fmt;
                    p.avAudioFrame->sample_rate = p.avAudioCodecContext->sample_rate;
                    p.avAudioFrame->nb_samples = frameSize;
                    r = av_channel_layout_copy(
                        &p.avAudioFrame->ch_layout,
                        &p.avAudioCodecContext->ch_layout);
                    if (r < 0)
                    {
                        throw std::runtime_error(ftk::Format("{0}: \"{1}\"").arg(getErrorLabel(r)).arg(p.fileName));
                    }
                    r = av_frame_get_buffer(p.avAudioFrame, 0);
                    if (r < 0)
                    {
                        throw std::runtime_error(ftk::Format("{0}: \"{1}\"").arg(getErrorLabel(r)).arg(p.fileName));
                    }
                }
            }

            // The file's own metadata only. The video and audio information
            // is kept out, so it does not land in the output as a set of
            // tags describing something that was read, not written. The
            // color description goes to the stream above, not here.
            for (const auto& i : info.tags)
            {
                if ("Color Primaries" == i.first ||
                    "Color Transfer" == i.first ||
                    "Color Matrix" == i.first)
                {
                    continue;
                }
                av_dict_set(&p.avFormatContext->metadata, i.first.c_str(), i.second.c_str(), 0);
            }

            //av_dump_format(p.avFormatContext, 0, p.fileName.c_str(), 1);

            r = avio_open(&p.avFormatContext->pb, p.fileName.c_str(), AVIO_FLAG_WRITE);
            if (r < 0)
            {
                throw std::runtime_error(ftk::Format("{0}: \"{1}\"").arg(getErrorLabel(r)).arg(p.fileName));
            }

            r = avformat_write_header(p.avFormatContext, NULL);
            if (r < 0)
            {
                throw std::runtime_error(ftk::Format("{0}: \"{1}\"").arg(getErrorLabel(r)).arg(p.fileName));
            }

            p.avPacket = av_packet_alloc();
            if (!p.avPacket)
            {
                throw std::runtime_error(ftk::Format("Cannot allocate packet: \"{0}\"").arg(p.fileName));
            }

            p.avFrame = av_frame_alloc();
            if (!p.avFrame)
            {
                throw std::runtime_error(ftk::Format("Cannot allocate frame: \"{0}\"").arg(p.fileName));
            }
            p.avFrame->format = p.avVideoStream->codecpar->format;
            p.avFrame->width = p.avVideoStream->codecpar->width;
            p.avFrame->height = p.avVideoStream->codecpar->height;
            r = av_frame_get_buffer(p.avFrame, 0);
            if (r < 0)
            {
                throw std::runtime_error(ftk::Format("{0}: \"{1}\"").arg(getErrorLabel(r)).arg(p.fileName));
            }

            p.avFrame2 = av_frame_alloc();
            if (!p.avFrame2)
            {
                throw std::runtime_error(ftk::Format("Cannot allocate frame: \"{0}\"").arg(p.fileName));
            }
            switch (videoInfo.type)
            {
            case ftk::ImageType::L_U8:     p.avPixelFormatIn = AV_PIX_FMT_GRAY8;  break;
            case ftk::ImageType::RGB_U8:   p.avPixelFormatIn = AV_PIX_FMT_RGB24;  break;
            case ftk::ImageType::RGBA_U8:  p.avPixelFormatIn = AV_PIX_FMT_RGBA;   break;
            case ftk::ImageType::L_U16:    p.avPixelFormatIn = AV_PIX_FMT_GRAY16; break;
            case ftk::ImageType::RGB_U16:  p.avPixelFormatIn = AV_PIX_FMT_RGB48;  break;
            case ftk::ImageType::RGBA_U16: p.avPixelFormatIn = AV_PIX_FMT_RGBA64; break;
            default:
                throw std::runtime_error(ftk::Format("Incompatible pixel type: \"{0}\"").arg(p.fileName));
                break;
            }
            p.swsContext = sws_alloc_context();
            if (!p.swsContext)
            {
                throw std::runtime_error(ftk::Format("Cannot allocate context: \"{0}\"").arg(p.fileName));
            }
            av_opt_set_defaults(p.swsContext);
            r = av_opt_set_int(p.swsContext, "srcw", videoInfo.size.w, AV_OPT_SEARCH_CHILDREN);
            r = av_opt_set_int(p.swsContext, "srch", videoInfo.size.h, AV_OPT_SEARCH_CHILDREN);
            r = av_opt_set_int(p.swsContext, "src_format", p.avPixelFormatIn, AV_OPT_SEARCH_CHILDREN);
            r = av_opt_set_int(p.swsContext, "dstw", videoInfo.size.w, AV_OPT_SEARCH_CHILDREN);
            r = av_opt_set_int(p.swsContext, "dsth", videoInfo.size.h, AV_OPT_SEARCH_CHILDREN);
            r = av_opt_set_int(p.swsContext, "dst_format", p.avCodecContext->pix_fmt, AV_OPT_SEARCH_CHILDREN);
            r = av_opt_set_int(p.swsContext, "sws_flags", swsScaleFlags, AV_OPT_SEARCH_CHILDREN);
            r = av_opt_set_int(p.swsContext, "threads", 0, AV_OPT_SEARCH_CHILDREN);
            r = sws_init_context(p.swsContext, nullptr, nullptr);
            if (r < 0)
            {
                throw std::runtime_error(ftk::Format("Cannot initialize sws context: \"{0}\"").arg(p.fileName));
            }

            p.opened = true;
        }

        Write::Write() :
            _p(new Private)
        {}

        Write::~Write()
        {
            FTK_P();

            try
            {
                finish();
            }
            catch (const std::exception& e)
            {
                // The destructor cannot throw; log the error instead.
                // Call finish() explicitly to have errors that occur
                // while finalizing the file reported as exceptions.
                if (auto logSystem = _logSystem.lock())
                {
                    logSystem->print(
                        "tl::ffmpeg::Write",
                        ftk::Format("Error finishing \"{0}\": {1}").
                            arg(p.fileName).arg(e.what()),
                        ftk::LogType::Error);
                }
            }

            if (p.avAudioFifo)
            {
                av_audio_fifo_free(p.avAudioFifo);
            }
            if (p.swrContext)
            {
                swr_free(&p.swrContext);
            }
            if (p.avAudioFrame)
            {
                av_frame_free(&p.avAudioFrame);
            }
            if (p.avAudioPacket)
            {
                av_packet_free(&p.avAudioPacket);
            }
            if (p.avAudioCodecContext)
            {
                avcodec_free_context(&p.avAudioCodecContext);
            }
            if (p.swsContext)
            {
                sws_freeContext(p.swsContext);
            }
            if (p.avFrame2)
            {
                av_frame_free(&p.avFrame2);
            }
            if (p.avFrame)
            {
                av_frame_free(&p.avFrame);
            }
            if (p.avPacket)
            {
                av_packet_free(&p.avPacket);
            }
            if (p.avCodecContext)
            {
                avcodec_free_context(&p.avCodecContext);
            }
            if (p.avFormatContext && p.avFormatContext->pb)
            {
                avio_closep(&p.avFormatContext->pb);
            }
            if (p.avFormatContext)
            {
                avformat_free_context(p.avFormatContext);
            }
        }

        std::shared_ptr<Write> Write::create(
            const ftk::Path& path,
            const IOInfo& info,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            auto out = std::shared_ptr<Write>(new Write);
            out->_init(path, info, options, logSystem);
            return out;
        }

        void Write::finish()
        {
            FTK_P();

            if (!p.opened || p.finished)
            {
                return;
            }
            // Mark as finished up front so that a failed finish is not
            // retried by the destructor.
            p.finished = true;

            // Flush the video encoder.
            _encodeVideo(nullptr);

            // Drain the audio resampler, then the FIFO (including a
            // final partial frame), then flush the audio encoder.
            if (p.avAudioCodecContext)
            {
                const int swrSamples = swr_get_out_samples(p.swrContext, 0);
                if (swrSamples > 0)
                {
                    uint8_t** swrData = nullptr;
                    int r = av_samples_alloc_array_and_samples(
                        &swrData,
                        nullptr,
                        p.avAudioCodecContext->ch_layout.nb_channels,
                        swrSamples,
                        p.avAudioCodecContext->sample_fmt,
                        0);
                    if (r >= 0)
                    {
                        const int converted = swr_convert(
                            p.swrContext,
                            swrData,
                            swrSamples,
                            nullptr,
                            0);
                        if (converted > 0)
                        {
                            av_audio_fifo_write(
                                p.avAudioFifo,
                                reinterpret_cast<void**>(swrData),
                                converted);
                        }
                        if (swrData)
                        {
                            av_freep(&swrData[0]);
                        }
                        av_freep(&swrData);
                    }
                }
                _drainAudioFifo(true);
                _encodeAudio(nullptr);
            }

            av_write_trailer(p.avFormatContext);
        }

        void Write::writeVideo(
            const OTIO_NS::RationalTime& time,
            const std::shared_ptr<ftk::Image>& image,
            const IOOptions&)
        {
            FTK_P();

            const auto& info = image->getInfo();
            av_image_fill_arrays(
                p.avFrame2->data,
                p.avFrame2->linesize,
                image->getData(),
                p.avPixelFormatIn,
                info.size.w,
                info.size.h,
                info.layout.alignment);

            // Flip the image vertically.
            switch (info.type)
            {
            case ftk::ImageType::L_U8:
            case ftk::ImageType::L_U16:
            case ftk::ImageType::RGB_U8:
            case ftk::ImageType::RGB_U16:
            case ftk::ImageType::RGBA_U8:
            case ftk::ImageType::RGBA_U16:
            {
                // Every type the plugin accepts is packed -- GRAY8, GRAY16,
                // RGB24, RGB48, RGBA, RGBA64 -- so the pixels are all in the
                // first plane whatever the depth, and that is the only one
                // there is to turn over.
                p.avFrame2->data[0] += p.avFrame2->linesize[0] * (info.size.h - 1);
                p.avFrame2->linesize[0] = -p.avFrame2->linesize[0];
                break;
            }
            case ftk::ImageType::YUV_420P_U8:
            case ftk::ImageType::YUV_422P_U8:
            case ftk::ImageType::YUV_444P_U8:
            case ftk::ImageType::YUV_420P_U16:
            case ftk::ImageType::YUV_422P_U16:
            case ftk::ImageType::YUV_444P_U16:
            {
                // Flip each plane by its own height. Chroma is vertically
                // subsampled (half height) only for 4:2:0; full height
                // otherwise.
                const bool halfChromaH =
                    ftk::ImageType::YUV_420P_U8  == info.type ||
                    ftk::ImageType::YUV_420P_U16 == info.type;
                const int planeH[3] = {
                    static_cast<int>(info.size.h),
                    static_cast<int>(halfChromaH ? info.size.h / 2 : info.size.h),
                    static_cast<int>(halfChromaH ? info.size.h / 2 : info.size.h) };
                for (int i = 0; i < 3; ++i)
                {
                    if (p.avFrame2->data[i] && p.avFrame2->linesize[i])
                    {
                        p.avFrame2->data[i] += p.avFrame2->linesize[i] * (planeH[i] - 1);
                        p.avFrame2->linesize[i] = -p.avFrame2->linesize[i];
                    }
                }
                break;
            }
            default: break;
            }

            sws_scale(
                p.swsContext,
                (uint8_t const* const*)p.avFrame2->data,
                p.avFrame2->linesize,
                0,
                p.avVideoStream->codecpar->height,
                p.avFrame->data,
                p.avFrame->linesize);

            const auto timeRational = toRational(time.rate());
            p.avFrame->pts = av_rescale_q(
                (time - _info.videoTime->start_time()).value(),
                { timeRational.second, timeRational.first },
                p.avVideoStream->time_base);
            _encodeVideo(p.avFrame);
        }

        void Write::writeAudio(
            const OTIO_NS::TimeRange&,
            const std::shared_ptr<Audio>& audio,
            const IOOptions&)
        {
            FTK_P();

            if (!p.avAudioCodecContext || !audio || !audio->isValid())
            {
                return;
            }

            // Convert the input audio to the encoder format.
            const int sampleCount = audio->getSampleCount();
            const uint8_t* data = audio->getData();
            const int swrSamples = swr_get_out_samples(p.swrContext, sampleCount);
            uint8_t** swrData = nullptr;
            int r = av_samples_alloc_array_and_samples(
                &swrData,
                nullptr,
                p.avAudioCodecContext->ch_layout.nb_channels,
                swrSamples,
                p.avAudioCodecContext->sample_fmt,
                0);
            if (r < 0)
            {
                throw std::runtime_error(ftk::Format("{0}: \"{1}\"").arg(getErrorLabel(r)).arg(p.fileName));
            }
            const int converted = swr_convert(
                p.swrContext,
                swrData,
                swrSamples,
                &data,
                sampleCount);
            if (converted > 0)
            {
                av_audio_fifo_write(
                    p.avAudioFifo,
                    reinterpret_cast<void**>(swrData),
                    converted);
            }
            if (swrData)
            {
                av_freep(&swrData[0]);
            }
            av_freep(&swrData);
            if (converted < 0)
            {
                throw std::runtime_error(ftk::Format("{0}: \"{1}\"").arg(getErrorLabel(converted)).arg(p.fileName));
            }

            _drainAudioFifo(false);
        }

        void Write::_drainAudioFifo(bool flush)
        {
            FTK_P();

            const int frameSize = p.avAudioCodecContext->frame_size > 0 ?
                p.avAudioCodecContext->frame_size :
                1024;
            while (av_audio_fifo_size(p.avAudioFifo) >= frameSize ||
                (flush && av_audio_fifo_size(p.avAudioFifo) > 0))
            {
                const int size = std::min(av_audio_fifo_size(p.avAudioFifo), frameSize);
                int r = av_frame_make_writable(p.avAudioFrame);
                if (r < 0)
                {
                    throw std::runtime_error(ftk::Format("{0}: \"{1}\"").arg(getErrorLabel(r)).arg(p.fileName));
                }
                p.avAudioFrame->nb_samples = size;
                r = av_audio_fifo_read(
                    p.avAudioFifo,
                    reinterpret_cast<void**>(p.avAudioFrame->data),
                    size);
                if (r < size)
                {
                    throw std::runtime_error(ftk::Format("Cannot read FIFO: \"{0}\"").arg(p.fileName));
                }
                p.avAudioFrame->pts = p.audioSampleCount;
                p.audioSampleCount += size;
                _encodeAudio(p.avAudioFrame);
            }
        }

        void Write::_encodeAudio(AVFrame* frame)
        {
            FTK_P();

            int r = avcodec_send_frame(p.avAudioCodecContext, frame);
            if (r < 0)
            {
                throw std::runtime_error(ftk::Format("Cannot write audio: \"{0}\"").arg(p.fileName));
            }

            while (r >= 0)
            {
                r = avcodec_receive_packet(p.avAudioCodecContext, p.avAudioPacket);
                if (r == AVERROR(EAGAIN) || r == AVERROR_EOF)
                {
                    return;
                }
                else if (r < 0)
                {
                    throw std::runtime_error(ftk::Format("Cannot write audio: \"{0}\"").arg(p.fileName));
                }
                p.avAudioPacket->stream_index = p.avAudioStream->index;
                av_packet_rescale_ts(
                    p.avAudioPacket,
                    p.avAudioCodecContext->time_base,
                    p.avAudioStream->time_base);
                r = av_interleaved_write_frame(p.avFormatContext, p.avAudioPacket);
                if (r < 0)
                {
                    throw std::runtime_error(ftk::Format("Cannot write audio: \"{0}\"").arg(p.fileName));
                }
                av_packet_unref(p.avAudioPacket);
            }
        }

        void Write::_encodeVideo(AVFrame* frame)
        {
            FTK_P();

            int r = avcodec_send_frame(p.avCodecContext, frame);
            if (r < 0)
            {
                throw std::runtime_error(ftk::Format("Cannot write frame: \"{0}\"").arg(p.fileName));
            }

            while (r >= 0)
            {
                r = avcodec_receive_packet(p.avCodecContext, p.avPacket);
                if (r == AVERROR(EAGAIN) || r == AVERROR_EOF)
                {
                    return;
                }
                else if (r < 0)
                {
                    throw std::runtime_error(ftk::Format("Cannot write frame: \"{0}\"").arg(p.fileName));
                }
                p.avPacket->stream_index = p.avVideoStream->index;
                r = av_interleaved_write_frame(p.avFormatContext, p.avPacket);
                if (r < 0)
                {
                    throw std::runtime_error(ftk::Format("Cannot write frame: \"{0}\"").arg(p.fileName));
                }
                av_packet_unref(p.avPacket);
            }
        }
    }
}

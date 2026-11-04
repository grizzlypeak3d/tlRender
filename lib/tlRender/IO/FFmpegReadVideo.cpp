// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/FFmpegReadPrivate.h>

#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>
#include <ftk/Core/OS.h>

#include <nlohmann/json.hpp>

#include <cstring>

extern "C"
{
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>

} // extern "C"

#if defined(__APPLE__)
#include <CoreVideo/CoreVideo.h>
#include <VideoToolbox/VideoToolbox.h>
#endif // __APPLE__

#include <algorithm>
#include <cmath>
#include <thread>

namespace tl
{
    namespace ffmpeg
    {
        namespace
        {
            //! Whether a decoder offers any hardware configuration that
            //! works through a device context.
            bool hasHwConfig(const AVCodec* codec)
            {
                for (int i = 0;; ++i)
                {
                    const AVCodecHWConfig* config =
                        avcodec_get_hw_config(codec, i);
                    if (!config)
                    {
                        break;
                    }
                    if (config->methods &
                        AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
                    {
                        return true;
                    }
                }
                return false;
            }

            //! The rate the frames actually come at, measured from the index
            //! -- the span of the decode times over the frames between
            //! them -- or, where there is no index, from the stream's
            //! average. Zero where neither says.
            double measuredRate(AVStream* stream)
            {
                const int count = avformat_index_get_entries_count(stream);
                if (count > 1)
                {
                    const AVIndexEntry* first = avformat_index_get_entry(stream, 0);
                    const AVIndexEntry* last = avformat_index_get_entry(stream, count - 1);
                    if (first && last && last->timestamp > first->timestamp)
                    {
                        return (count - 1) /
                            ((last->timestamp - first->timestamp) * av_q2d(stream->time_base));
                    }
                }
                if (stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0)
                {
                    return av_q2d(stream->avg_frame_rate);
                }
                return 0.0;
            }

            //! The standard rate a measured one is, if it is one.
            AVRational standardRate(double value)
            {
                for (const AVRational rate : {
                    AVRational{ 24000, 1001 }, AVRational{ 24, 1 },
                    AVRational{ 25, 1 },
                    AVRational{ 30000, 1001 }, AVRational{ 30, 1 },
                    AVRational{ 48000, 1001 }, AVRational{ 48, 1 },
                    AVRational{ 50, 1 },
                    AVRational{ 60000, 1001 }, AVRational{ 60, 1 } })
                {
                    if (std::abs(value - av_q2d(rate)) / av_q2d(rate) < .005)
                    {
                        return rate;
                    }
                }
                return AVRational{ 0, 1 };
            }

            //! Whether frames of one format are copied straight into images
            //! of the other: the same format, and one laid out the way
            //! ftk::Image has it -- packed 8-bit gray, RGB and RGBA, or
            //! planar YUV without alpha in 8 or 16-bit words. The 10 and
            //! 12-bit formats are among the latter, kept as decoded.
            bool canCopy(AVPixelFormat in, AVPixelFormat out)
            {
                bool r = false;
                if (in == out)
                {
                    r = AV_PIX_FMT_RGB24 == in ||
                        AV_PIX_FMT_GRAY8 == in ||
                        AV_PIX_FMT_RGBA  == in;
                    if (!r)
                    {
                        const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(in);
                        r = desc &&
                            (desc->flags & AV_PIX_FMT_FLAG_PLANAR) &&
                            !(desc->flags & AV_PIX_FMT_FLAG_BE) &&
                            !(desc->flags & AV_PIX_FMT_FLAG_ALPHA) &&
                            !(desc->flags & AV_PIX_FMT_FLAG_RGB) &&
                            !(desc->flags & AV_PIX_FMT_FLAG_BITSTREAM) &&
                            !(desc->flags & AV_PIX_FMT_FLAG_PAL) &&
                            3 == desc->nb_components &&
                            (1 == desc->comp[0].step || 2 == desc->comp[0].step);
                    }
                }
                return r;
            }

            //! Copy the planes of a frame into an image, the rows shared out
            //! among the cores when there is enough to share: an 8K 4:4:4
            //! 12-bit frame is 190 MB, ten milliseconds for one core, half
            //! of a frame at 48 frames a second. (swscale would make the
            //! same copy on one thread whatever its thread count: its
            //! unscaled converters are not sliced.)
            void copyPlanes(
                const AVFrame* frame,
                uint8_t* data,
                AVPixelFormat format,
                int w,
                int h)
            {
                const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(format);
                uint8_t* dstData[4] = { nullptr, nullptr, nullptr, nullptr };
                int dstLinesize[4] = { 0, 0, 0, 0 };
                av_image_fill_arrays(dstData, dstLinesize, data, format, w, h, 1);
                struct Plane
                {
                    const uint8_t* src;
                    int srcLinesize;
                    uint8_t* dst;
                    int dstLinesize;
                    int bytes;
                    int h;
                };
                std::vector<Plane> planes;
                int rowsTotal = 0;
                size_t bytesTotal = 0;
                for (int p = 0; p < 3; ++p)
                {
                    const int pw = p ? AV_CEIL_RSHIFT(w, desc->log2_chroma_w) : w;
                    const int ph = p ? AV_CEIL_RSHIFT(h, desc->log2_chroma_h) : h;
                    const int bytes = pw * desc->comp[0].step;
                    planes.push_back({ frame->data[p], frame->linesize[p], dstData[p], dstLinesize[p], bytes, ph });
                    rowsTotal += ph;
                    bytesTotal += static_cast<size_t>(bytes) * ph;
                }
                // The rows of all three planes, numbered through.
                const auto work = [&planes](int r0, int r1)
                {
                    int base = 0;
                    for (const Plane& pl : planes)
                    {
                        const int y0 = std::max(r0, base) - base;
                        const int y1 = std::min(r1, base + pl.h) - base;
                        for (int y = y0; y < y1; ++y)
                        {
                            std::memcpy(
                                pl.dst + static_cast<ptrdiff_t>(pl.dstLinesize) * y,
                                pl.src + static_cast<ptrdiff_t>(pl.srcLinesize) * y,
                                pl.bytes);
                        }
                        base += pl.h;
                    }
                };
                // A thread for every 8 MB, so that a 1080p frame is copied
                // here without starting any.
                const int threadCount = std::max(1, std::min({
                    16,
                    static_cast<int>(std::thread::hardware_concurrency()),
                    static_cast<int>(bytesTotal / (8 * 1024 * 1024)) }));
                const int chunk = std::max(1, (rowsTotal + threadCount - 1) / threadCount);
                std::vector<std::thread> threads;
                for (int r0 = chunk; r0 < rowsTotal; r0 += chunk)
                {
                    threads.emplace_back(work, r0, std::min(rowsTotal, r0 + chunk));
                }
                // This thread takes the first chunk rather than only waiting.
                work(0, std::min(rowsTotal, chunk));
                for (auto& t : threads)
                {
                    t.join();
                }
            }

            //! Whether a format is YUV with the two chroma components
            //! interleaved in a second plane, as hardware hands frames back:
            //! NV12 and P010 for 4:2:0, NV16 and P210 for 4:2:2, NV24 and
            //! P416 for 4:4:4.
            bool isSemiPlanar(AVPixelFormat format)
            {
                const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(format);
                return desc &&
                    (desc->flags & AV_PIX_FMT_FLAG_PLANAR) &&
                    !(desc->flags & AV_PIX_FMT_FLAG_BE) &&
                    !(desc->flags & AV_PIX_FMT_FLAG_ALPHA) &&
                    !(desc->flags & AV_PIX_FMT_FLAG_RGB) &&
                    3 == desc->nb_components &&
                    1 == desc->comp[1].plane &&
                    1 == desc->comp[2].plane;
            }

            //! Whether a semi-planar frame can be copied into an image of a
            //! planar format by copySemiPlanar(): the same chroma
            //! subsampling, and samples of the same size.
            bool canCopySemiPlanar(AVPixelFormat in, AVPixelFormat out)
            {
                if (!isSemiPlanar(in) || isSemiPlanar(out))
                {
                    return false;
                }
                const AVPixFmtDescriptor* inDesc = av_pix_fmt_desc_get(in);
                const AVPixFmtDescriptor* outDesc = av_pix_fmt_desc_get(out);
                return outDesc &&
                    (outDesc->flags & AV_PIX_FMT_FLAG_PLANAR) &&
                    !(outDesc->flags & AV_PIX_FMT_FLAG_BE) &&
                    !(outDesc->flags & AV_PIX_FMT_FLAG_ALPHA) &&
                    !(outDesc->flags & AV_PIX_FMT_FLAG_RGB) &&
                    3 == outDesc->nb_components &&
                    inDesc->log2_chroma_w == outDesc->log2_chroma_w &&
                    inDesc->log2_chroma_h == outDesc->log2_chroma_h &&
                    (inDesc->comp[0].step > 1) == (outDesc->comp[0].step > 1);
            }

            //! Copy a semi-planar frame into a planar image of the same
            //! chroma subsampling: the luma as it is, the chroma split into
            //! its two planes, and each sample brought to the depth the
            //! image keeps. The hardware's samples sit in the high bits of
            //! their words (P210), the software decoder's in the low bits,
            //! so that a hardware frame lands exactly as the software
            //! decoder's would and is drawn the same way. Rows are shared
            //! among the cores as copyPlanes() shares them.
            void copySemiPlanar(
                const AVFrame* frame,
                uint8_t* data,
                AVPixelFormat format,
                int w,
                int h)
            {
                const AVPixFmtDescriptor* srcDesc =
                    av_pix_fmt_desc_get(static_cast<AVPixelFormat>(frame->format));
                const AVPixFmtDescriptor* dstDesc = av_pix_fmt_desc_get(format);
                uint8_t* dstData[4] = { nullptr, nullptr, nullptr, nullptr };
                int dstLinesize[4] = { 0, 0, 0, 0 };
                av_image_fill_arrays(dstData, dstLinesize, data, format, w, h, 1);
                const int cw = AV_CEIL_RSHIFT(w, dstDesc->log2_chroma_w);
                const int ch = AV_CEIL_RSHIFT(h, dstDesc->log2_chroma_h);
                const bool wide = srcDesc->comp[0].step > 1;
                const int srcShift = srcDesc->comp[0].shift;
                const int srcDepth = srcDesc->comp[0].depth;
                const int dstDepth = dstDesc->comp[0].depth;
                const int dstMax = (1 << dstDepth) - 1;
                // From the source's depth to the image's, rounded.
                const auto sample = [srcShift, srcDepth, dstDepth, dstMax](uint16_t s)
                {
                    int v = s >> srcShift;
                    if (srcDepth > dstDepth)
                    {
                        const int d = srcDepth - dstDepth;
                        v = std::min(dstMax, (v + (1 << (d - 1))) >> d);
                    }
                    else if (srcDepth < dstDepth)
                    {
                        v <<= dstDepth - srcDepth;
                    }
                    return static_cast<uint16_t>(v);
                };
                const auto lumaRow = [&](int y)
                {
                    const uint8_t* src = frame->data[0] + static_cast<ptrdiff_t>(frame->linesize[0]) * y;
                    uint8_t* dst = dstData[0] + static_cast<ptrdiff_t>(dstLinesize[0]) * y;
                    if (!wide)
                    {
                        std::memcpy(dst, src, w);
                    }
                    else
                    {
                        const uint16_t* s = reinterpret_cast<const uint16_t*>(src);
                        uint16_t* d = reinterpret_cast<uint16_t*>(dst);
                        for (int x = 0; x < w; ++x)
                        {
                            d[x] = sample(s[x]);
                        }
                    }
                };
                const auto chromaRow = [&](int y)
                {
                    const uint8_t* src = frame->data[1] + static_cast<ptrdiff_t>(frame->linesize[1]) * y;
                    uint8_t* u = dstData[1] + static_cast<ptrdiff_t>(dstLinesize[1]) * y;
                    uint8_t* v = dstData[2] + static_cast<ptrdiff_t>(dstLinesize[2]) * y;
                    if (!wide)
                    {
                        for (int x = 0; x < cw; ++x)
                        {
                            u[x] = src[x * 2];
                            v[x] = src[x * 2 + 1];
                        }
                    }
                    else
                    {
                        const uint16_t* s = reinterpret_cast<const uint16_t*>(src);
                        uint16_t* u16 = reinterpret_cast<uint16_t*>(u);
                        uint16_t* v16 = reinterpret_cast<uint16_t*>(v);
                        for (int x = 0; x < cw; ++x)
                        {
                            u16[x] = sample(s[x * 2]);
                            v16[x] = sample(s[x * 2 + 1]);
                        }
                    }
                };
                // The luma rows, then the chroma rows, numbered through.
                const int rowsTotal = h + ch;
                const auto work = [&](int r0, int r1)
                {
                    for (int r = r0; r < r1; ++r)
                    {
                        if (r < h)
                        {
                            lumaRow(r);
                        }
                        else
                        {
                            chromaRow(r - h);
                        }
                    }
                };
                const size_t bytesTotal =
                    (static_cast<size_t>(w) * h + static_cast<size_t>(cw) * ch * 2) * (wide ? 2 : 1);
                const int threadCount = std::max(1, std::min({
                    16,
                    static_cast<int>(std::thread::hardware_concurrency()),
                    static_cast<int>(bytesTotal / (8 * 1024 * 1024)) }));
                const int chunk = std::max(1, (rowsTotal + threadCount - 1) / threadCount);
                std::vector<std::thread> threads;
                for (int r0 = chunk; r0 < rowsTotal; r0 += chunk)
                {
                    threads.emplace_back(work, r0, std::min(rowsTotal, r0 + chunk));
                }
                work(0, std::min(rowsTotal, chunk));
                for (auto& t : threads)
                {
                    t.join();
                }
            }

            //! Whether a codec is always decoded by the platform's own
            //! decoder: ProRes on Apple's systems, whose decoder is the
            //! reference for the format (FFmpeg's is not licensed by Apple),
            //! whether or not hardware decoding is on and whether or not the
            //! machine has ProRes hardware -- without it VideoToolbox
            //! decodes in software, which is still Apple's decoder.
            bool isPlatformDecoded(AVCodecID id)
            {
#if defined(__APPLE__)
                return AV_CODEC_ID_PRORES == id;
#else
                (void)id;
                return false;
#endif // __APPLE__
            }

            //! Whether a decoder only decodes through hardware. FFmpeg's own
            //! AV1 decoder parses the stream and hands every frame to a
            //! hardware accelerator; without one each frame fails with
            //! "Function not implemented". It is the only AV1 decoder where
            //! libaom is not built, the iPad among them, and Apple's
            //! hardware decodes AV1 from the M3 and the A17 Pro on.
            bool isHardwareOnly(const AVCodec* codec)
            {
                return AV_CODEC_ID_AV1 == codec->id &&
                    codec->name &&
                    0 == strcmp(codec->name, "av1");
            }
        }

        ReadVideo::ReadVideo(
            const std::string& fileName,
            const std::vector<ftk::MemFile>& memory,
            const ReadOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem) :
            _fileName(fileName),
            _options(options),
            _logSystem(logSystem)
        {
            try
            {
                // A window on a bundle that could not be mapped is opened
                // through the file, by the subfile URL.
                const bool window = isWindow(memory);
                const std::string openName = window ? getSubfileName(memory) : fileName;
                if (!memory.empty() && !window)
                {
                    _avFormatContext = avformat_alloc_context();
                    if (!_avFormatContext)
                    {
                        throw std::runtime_error(
                            ftk::Format("Cannot allocate format context: \"{0}\"").
                            arg(fileName));
                    }

                    _avIOBufferData = AVIOBufferData(memory[0].p, memory[0].size);
                    _avIOContextBuffer = static_cast<uint8_t*>(av_malloc(avIOContextBufferSize));
                    _avIOContext = avio_alloc_context(
                        _avIOContextBuffer,
                        avIOContextBufferSize,
                        0,
                        &_avIOBufferData,
                        &avIOBufferRead,
                        nullptr,
                        &avIOBufferSeek);
                    if (!_avIOContext)
                    {
                        throw std::runtime_error(
                            ftk::Format("Cannot allocate I/O context: \"{0}\"").
                            arg(fileName));
                    }

                    _avFormatContext->pb = _avIOContext;
                }

                int r = avformat_open_input(
                    &_avFormatContext,
                    !_avFormatContext ? openName.c_str() : nullptr,
                    nullptr,
                    nullptr);
                if (r < 0)
                {
                    throw std::runtime_error(
                        ftk::Format("{0}: \"{1}\"").
                        arg(getErrorLabel(r)).
                        arg(fileName));
                }

                r = avformat_find_stream_info(_avFormatContext, nullptr);
                if (r < 0)
                {
                    throw std::runtime_error(
                        ftk::Format("{0}: \"{1}\"").
                        arg(getErrorLabel(r)).
                        arg(fileName));
                }
                //for (unsigned int i = 0; i < _avFormatContext->nb_streams; ++i)
                //    av_dump_format(_avFormatContext, i, fileName.c_str(), 0);
                _avStream = findStream(_avFormatContext, AVMEDIA_TYPE_VIDEO);
                const std::string timecode = getTimecode(_avFormatContext);
                // Only the video stream is read: the demuxer then skips the
                // others' data rather than handing it over to be thrown away.
                // The timecode and the other streams' parameters are
                // metadata, found already.
                for (unsigned int i = 0; i < _avFormatContext->nb_streams; ++i)
                {
                    if (static_cast<int>(i) != _avStream)
                    {
                        _avFormatContext->streams[i]->discard = AVDISCARD_ALL;
                    }
                }
                if (_avStream != -1)
                {
                    //av_dump_format(_avFormatContext, _avStream, fileName.c_str(), 0);

                    auto avVideoStream = _avFormatContext->streams[_avStream];
                    auto avVideoCodecParameters = avVideoStream->codecpar;
                    const AVCodec* avVideoCodecDefault =
                        avcodec_find_decoder(avVideoCodecParameters->codec_id);
                    if (!avVideoCodecDefault)
                    {
                        throw std::runtime_error(
                            ftk::Format("No video codec found: \"{0}\"").
                            arg(fileName));
                    }
                    _avCodecDefault = avVideoCodecDefault;
                    _textures = options.textures;
                    const AVCodec* avVideoCodec = avVideoCodecDefault;
                    const bool hwAccel = options.hwAccel ||
                        isPlatformDecoded(avVideoCodecParameters->codec_id);
                    if (hwAccel && !hasHwConfig(avVideoCodecDefault))
                    {
                        // The default decoder for a codec can be a software
                        // library with no hardware configurations at all --
                        // FFmpeg answers AV1 with libaom -- while the
                        // hardware-capable decoder sits behind it (#833). If
                        // the hardware then fails to come up, the default is
                        // put back below.
                        void* i = nullptr;
                        while (const AVCodec* candidate = av_codec_iterate(&i))
                        {
                            if (av_codec_is_decoder(candidate) &&
                                candidate->id == avVideoCodecParameters->codec_id &&
                                hasHwConfig(candidate))
                            {
                                avVideoCodec = candidate;
                                break;
                            }
                        }
                    }
                    _avCodecParameters[_avStream] = avcodec_parameters_alloc();
                    if (!_avCodecParameters[_avStream])
                    {
                        throw std::runtime_error(
                            ftk::Format("Cannot allocate parameters: \"{0}\"").
                            arg(fileName));
                    }
                    r = avcodec_parameters_copy(_avCodecParameters[_avStream], avVideoCodecParameters);
                    if (r < 0)
                    {
                        throw std::runtime_error(
                            ftk::Format("{0}: \"{1}\"").
                            arg(getErrorLabel(r)).
                            arg(fileName));
                    }
                    _avCodec = avVideoCodec;
                    r = _openCodec(avVideoCodec, hwAccel);
                    if (avVideoCodec != avVideoCodecDefault &&
                        (r < 0 || !_hwAccel))
                    {
                        // The alternative decoder was chosen only for its
                        // hardware; without it -- the device did not come up,
                        // or the codec did not open -- the default software
                        // decoder is the right one after all.
                        if (_hwDeviceContext)
                        {
                            av_buffer_unref(&_hwDeviceContext);
                        }
                        _hwAccel = false;
                        _hwPixelFormat = AV_PIX_FMT_NONE;
                        _avCodec = avVideoCodecDefault;
                        r = _openCodec(_avCodec, false);
                    }
                    if (r < 0)
                    {
                        throw std::runtime_error(
                            ftk::Format("{0}: \"{1}\"").
                            arg(getErrorLabel(r)).
                            arg(fileName));
                    }
                    if (!_hwAccel && isHardwareOnly(_avCodec))
                    {
                        // Refused here rather than opened to fail at every
                        // frame, which left an empty viewer: the machine has
                        // no AV1 hardware, or the gates in _initHwAccel()
                        // kept this file off it. Kept short, since the
                        // application shows it over the picture, elided.
                        throw std::runtime_error(
                            ftk::Format("Cannot decode this AV1 file without hardware decoding: \"{0}\"").
                            arg(fileName));
                    }

                    _info.size.w = _avCodecParameters[_avStream]->width;
                    _info.size.h = _avCodecParameters[_avStream]->height;
                    // Asked of the format rather than read from the codec
                    // parameters: what a QuickTime carries in its "pasp" atom
                    // reaches the stream, and the parameters copied from the
                    // codec do not have it. An anamorphic movie came back
                    // square, which is what the command line -- ffprobe
                    // reports the stream -- did not do.
                    const AVRational sampleAspectRatio =
                        av_guess_sample_aspect_ratio(
                            _avFormatContext,
                            _avFormatContext->streams[_avStream],
                            nullptr);
                    if (sampleAspectRatio.num > 0 && sampleAspectRatio.den > 0)
                    {
                        _info.pixelAspectRatio = av_q2d(sampleAspectRatio);
                    }
                    _info.layout.mirror.y = true;

                    _avInputPixelFormat = static_cast<AVPixelFormat>(_avCodecParameters[_avStream]->format);
                    // Planar 10 and 12-bit frames are kept as decoded, the
                    // values in the low bits of 16-bit words with the depth
                    // in the image information (ftk::ImageInfo::depth) for
                    // the renderers to scale by, so that the copy out of the
                    // decoder is a plain copy. (Widening 8K 4:4:4 12-bit
                    // frames cost as much as decoding them.) Big-endian
                    // frames, rare in files, are still converted to 16-bit
                    // little-endian.
                    const auto keepPlanar = [this](AVPixelFormat p16le, ftk::ImageType type)
                    {
                        const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(_avInputPixelFormat);
                        if (desc &&
                            !(desc->flags & AV_PIX_FMT_FLAG_BE) &&
                            desc->comp[0].depth < 16)
                        {
                            _avOutputPixelFormat = _avInputPixelFormat;
                            _info.depth = desc->comp[0].depth;
                        }
                        else
                        {
                            _avOutputPixelFormat = p16le;
                        }
                        _info.type = type;
                    };
                    switch (_avInputPixelFormat)
                    {
                    case AV_PIX_FMT_RGB24:
                        _avOutputPixelFormat = _avInputPixelFormat;
                        _info.type = ftk::ImageType::RGB_U8;
                        break;
                    case AV_PIX_FMT_GRAY8:
                        _avOutputPixelFormat = _avInputPixelFormat;
                        _info.type = ftk::ImageType::L_U8;
                        break;
                    case AV_PIX_FMT_RGBA:
                        _avOutputPixelFormat = _avInputPixelFormat;
                        _info.type = ftk::ImageType::RGBA_U8;
                        break;
                    case AV_PIX_FMT_YUV420P:
                        if (options.yuvToRGBConversion)
                        {
                            _avOutputPixelFormat = AV_PIX_FMT_RGB24;
                            _info.type = ftk::ImageType::RGB_U8;
                        }
                        else
                        {
                            _avOutputPixelFormat = _avInputPixelFormat;
                            _info.type = ftk::ImageType::YUV_420P_U8;
                        }
                        break;
                    case AV_PIX_FMT_YUV422P:
                        if (options.yuvToRGBConversion)
                        {
                            _avOutputPixelFormat = AV_PIX_FMT_RGB24;
                            _info.type = ftk::ImageType::RGB_U8;
                        }
                        else
                        {
                            _avOutputPixelFormat = _avInputPixelFormat;
                            _info.type = ftk::ImageType::YUV_422P_U8;
                        }
                        break;
                    case AV_PIX_FMT_YUV444P:
                        if (options.yuvToRGBConversion)
                        {
                            _avOutputPixelFormat = AV_PIX_FMT_RGB24;
                            _info.type = ftk::ImageType::RGB_U8;
                        }
                        else
                        {
                            _avOutputPixelFormat = _avInputPixelFormat;
                            _info.type = ftk::ImageType::YUV_444P_U8;
                        }
                        break;
                    case AV_PIX_FMT_YUV420P10BE:
                    case AV_PIX_FMT_YUV420P10LE:
                    case AV_PIX_FMT_YUV420P12BE:
                    case AV_PIX_FMT_YUV420P12LE:
                    case AV_PIX_FMT_YUV420P16BE:
                    case AV_PIX_FMT_YUV420P16LE:
                        if (options.yuvToRGBConversion)
                        {
                            _avOutputPixelFormat = AV_PIX_FMT_RGB48;
                            _info.type = ftk::ImageType::RGB_U16;
                        }
                        else
                        {
                            keepPlanar(AV_PIX_FMT_YUV420P16LE, ftk::ImageType::YUV_420P_U16);
                        }
                        break;
                    case AV_PIX_FMT_YUV422P10BE:
                    case AV_PIX_FMT_YUV422P10LE:
                    case AV_PIX_FMT_YUV422P12BE:
                    case AV_PIX_FMT_YUV422P12LE:
                    case AV_PIX_FMT_YUV422P16BE:
                    case AV_PIX_FMT_YUV422P16LE:
                        if (options.yuvToRGBConversion)
                        {
                            _avOutputPixelFormat = AV_PIX_FMT_RGB48;
                            _info.type = ftk::ImageType::RGB_U16;
                        }
                        else
                        {
                            keepPlanar(AV_PIX_FMT_YUV422P16LE, ftk::ImageType::YUV_422P_U16);
                        }
                        break;
                    case AV_PIX_FMT_YUV444P10BE:
                    case AV_PIX_FMT_YUV444P10LE:
                    case AV_PIX_FMT_YUV444P12BE:
                    case AV_PIX_FMT_YUV444P12LE:
                    case AV_PIX_FMT_YUV444P16BE:
                    case AV_PIX_FMT_YUV444P16LE:
                        if (options.yuvToRGBConversion)
                        {
                            _avOutputPixelFormat = AV_PIX_FMT_RGB48;
                            _info.type = ftk::ImageType::RGB_U16;
                        }
                        else
                        {
                            keepPlanar(AV_PIX_FMT_YUV444P16LE, ftk::ImageType::YUV_444P_U16);
                        }
                        break;
                    case AV_PIX_FMT_GBRP:
                    case AV_PIX_FMT_BGR24:
                    case AV_PIX_FMT_BGR0:
                    case AV_PIX_FMT_RGB0:
                        // RGB in another layout, as FFV1 stores it: converted
                        // to RGB rather than falling to the default below,
                        // which takes it for YUV and loses what a lossless
                        // file kept.
                        _avOutputPixelFormat = AV_PIX_FMT_RGB24;
                        _info.type = ftk::ImageType::RGB_U8;
                        break;
                    case AV_PIX_FMT_BGRA:
                    case AV_PIX_FMT_ARGB:
                    case AV_PIX_FMT_ABGR:
                    case AV_PIX_FMT_PAL8:
                        // RGB with alpha in another layout, or through a
                        // palette, as a GIF is decoded: likewise converted
                        // to what it is rather than taken for YUV, which
                        // halved the color's resolution.
                        _avOutputPixelFormat = AV_PIX_FMT_RGBA;
                        _info.type = ftk::ImageType::RGBA_U8;
                        break;
                    case AV_PIX_FMT_GBRP9BE:
                    case AV_PIX_FMT_GBRP9LE:
                    case AV_PIX_FMT_GBRP10BE:
                    case AV_PIX_FMT_GBRP10LE:
                    case AV_PIX_FMT_GBRP12BE:
                    case AV_PIX_FMT_GBRP12LE:
                    case AV_PIX_FMT_GBRP14BE:
                    case AV_PIX_FMT_GBRP14LE:
                    case AV_PIX_FMT_GBRP16BE:
                    case AV_PIX_FMT_GBRP16LE:
                    case AV_PIX_FMT_RGB48BE:
                    case AV_PIX_FMT_RGB48LE:
                        _avOutputPixelFormat = AV_PIX_FMT_RGB48;
                        _info.type = ftk::ImageType::RGB_U16;
                        break;
                    case AV_PIX_FMT_YUVA420P:
                    case AV_PIX_FMT_YUVA422P:
                    case AV_PIX_FMT_YUVA444P:
                        //! \todo Support these formats natively.
                        _avOutputPixelFormat = AV_PIX_FMT_RGBA;
                        _info.type = ftk::ImageType::RGBA_U8;
                        break;
                    case AV_PIX_FMT_YUVA444P10BE:
                    case AV_PIX_FMT_YUVA444P10LE:
                    case AV_PIX_FMT_YUVA444P12BE:
                    case AV_PIX_FMT_YUVA444P12LE:
                    case AV_PIX_FMT_YUVA444P16BE:
                    case AV_PIX_FMT_YUVA444P16LE:
                        //! \todo Support these formats natively.
                        _avOutputPixelFormat = AV_PIX_FMT_RGBA64;
                        _info.type = ftk::ImageType::RGBA_U16;
                        break;
                    default:
                        if (options.yuvToRGBConversion)
                        {
                            _avOutputPixelFormat = AV_PIX_FMT_RGB24;
                            _info.type = ftk::ImageType::RGB_U8;
                        }
                        else
                        {
                            _avOutputPixelFormat = AV_PIX_FMT_YUV420P;
                            _info.type = ftk::ImageType::YUV_420P_U8;
                        }
                        break;
                    }
                    // Hardware frames download semi-planar: NV12 or P010 for
                    // 4:2:0, P210 for 4:2:2, P416 for 4:4:4. 8-bit 4:2:0 is
                    // kept as NV12, handed to the display shader as it came
                    // (and on Apple's systems drawn from VideoToolbox's own
                    // texture, see _textures). Everything else is copied into
                    // the image the software decoder would have made, its
                    // chroma split into planes and its samples at the same
                    // depth (copySemiPlanar()), so the two decoders' frames
                    // are drawn alike: P010 kept as it came had its samples
                    // in the high bits and was taken for ten bits in the low
                    // ones, and drew blown out. _avOutputPixelFormat doubles
                    // as the sws fallback target for any other download
                    // format (see _copy()).
                    if (_hwAccel)
                    {
                        const AVPixFmtDescriptor* desc =
                            av_pix_fmt_desc_get(_avInputPixelFormat);
                        if (desc &&
                            8 == desc->comp[0].depth &&
                            1 == desc->log2_chroma_w &&
                            1 == desc->log2_chroma_h &&
                            !(desc->flags & AV_PIX_FMT_FLAG_ALPHA))
                        {
                            _avOutputPixelFormat = AV_PIX_FMT_NV12;
                            _info.type = ftk::ImageType::YUV_420SP_U8;
                            _info.depth = 0;
                        }
                    }
                    if (!isFullRange(
                        _avCodecContext[_avStream]->color_range,
                        _avInputPixelFormat))
                    {
                        _info.videoLevels = ftk::VideoLevels::LegalRange;
                    }
                    const bool convertedToRGB =
                        _avInputPixelFormat != _avOutputPixelFormat &&
                        (AV_PIX_FMT_RGB24  == _avOutputPixelFormat ||
                         AV_PIX_FMT_RGB48  == _avOutputPixelFormat ||
                         AV_PIX_FMT_RGBA   == _avOutputPixelFormat ||
                         AV_PIX_FMT_RGBA64 == _avOutputPixelFormat);
                    if (convertedToRGB)
                    {
                        _info.videoLevels = ftk::VideoLevels::FullRange;
                    }
                    _info.yuvCoefficients = toYUVCoefficients(
                        _avCodecParameters[_avStream]->color_space,
                        _avInputPixelFormat,
                        _info.size);

                    // The rate: FFmpeg's guess, unless the frames say
                    // otherwise. Film with 3:2 pulldown kept in the
                    // timestamps -- a DVD remuxed into MP4 -- has 23.976
                    // frames a second, each lasting three fields and then
                    // two; the nominal rate is the field rate, and the
                    // guess the video rate of the MPEG-2 header. Neither
                    // fits the frames, and on either a frame in eight
                    // landed on its neighbor's tick. The rate the frames
                    // come at, when it is a standard one, does: every
                    // frame is then within a field of its own tick.
                    _avSpeed = av_guess_frame_rate(_avFormatContext, avVideoStream, nullptr);
                    bool measured = false;
                    {
                        const AVRational rate = standardRate(measuredRate(avVideoStream));
                        if (rate.num > 0 &&
                            std::abs(av_q2d(rate) - av_q2d(_avSpeed)) / av_q2d(rate) >= .005)
                        {
                            _log(ftk::Format("Frame rate {0}, from the frames, rather than {1}: \"{2}\"").
                                arg(av_q2d(rate)).
                                arg(av_q2d(_avSpeed)).
                                arg(fileName));
                            _avSpeed = rate;
                            measured = true;
                            _measuredRate = true;
                        }
                    }
                    const double speed = av_q2d(_avSpeed);

                    // Every time is in ticks of that rate, from here to the
                    // frames' times: taken at the nominal rate instead and
                    // labelled with this one, a seek landed where it was
                    // asked to and then the reader handed out a frame a
                    // tick, running ahead until the next seek pulled it
                    // back.
                    std::size_t sequenceSize = 0;
                    if (avVideoStream->nb_frames > 0 && !measured)
                    {
                        // The count, and no less than the length: a
                        // movie that skips frames -- a phone's, a screen
                        // recording, one that dropped frames -- spans
                        // more ticks than it has frames, and by its count
                        // alone its end was cut off.
                        sequenceSize = avVideoStream->nb_frames;
                        if (avVideoStream->duration != AV_NOPTS_VALUE)
                        {
                            sequenceSize = std::max(
                                sequenceSize,
                                static_cast<std::size_t>(av_rescale_q(
                                    avVideoStream->duration,
                                    avVideoStream->time_base,
                                    swap(_avSpeed))));
                        }
                    }
                    else if (measured && avVideoStream->duration != AV_NOPTS_VALUE)
                    {
                        // From the length rather than the count when the
                        // rate is the frames': a disc that mixes film with
                        // video has more frames than film ticks. Rounded
                        // up, since the length ends with the last frame's
                        // packet duration, which in such a file is a
                        // field's rather than a frame's, and rounded to the
                        // nearest tick it left the last frame out.
                        // From the start of the file, since the video
                        // can start a few fields in.
                        const int64_t end = avVideoStream->duration +
                            (avVideoStream->start_time != AV_NOPTS_VALUE ?
                                avVideoStream->start_time : 0);
                        sequenceSize = av_rescale_q_rnd(
                            end,
                            avVideoStream->time_base,
                            swap(_avSpeed),
                            AV_ROUND_UP);
                    }
                    else if (avVideoStream->duration != AV_NOPTS_VALUE)
                    {
                        sequenceSize = av_rescale_q(
                            avVideoStream->duration,
                            avVideoStream->time_base,
                            swap(_avSpeed));
                    }
                    else if (_avFormatContext->duration != AV_NOPTS_VALUE)
                    {
                        sequenceSize = av_rescale_q(
                            _avFormatContext->duration,
                            av_get_time_base_q(),
                            swap(_avSpeed));
                    }
        
                    ftk::ImageTags tags;
                    AVDictionaryEntry* tag = nullptr;
                    while ((tag = av_dict_get(_avFormatContext->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
                    {
                        const std::string key(tag->key);
                        const std::string value(tag->value);
                        tags[key] = value;
                    }

                    OTIO_NS::RationalTime startTime(0.0, speed);
                    if (!timecode.empty())
                    {
                        opentime::ErrorStatus errorStatus;
                        const OTIO_NS::RationalTime time = OTIO_NS::RationalTime::from_timecode(
                            timecode,
                            speed,
                            &errorStatus);
                        if (!opentime::is_error(errorStatus))
                        {
                            startTime = time.floor();
                        }
                    }
                    _timeRange = OTIO_NS::TimeRange(
                        startTime,
                        OTIO_NS::RationalTime(sequenceSize, speed));

                    for (const auto& i : tags)
                    {
                        _tags[i.first] = i.second;
                    }
                    // Kept where a stream had it, for whether it is drop
                    // frame.
                    if (!timecode.empty())
                    {
                        _tags["timecode"] = timecode;
                    }

                    // The stream's color description, for resolving an
                    // input color space from what the file itself says.
                    // Only what was actually flagged; "unspecified" says
                    // nothing worth repeating.
                    const AVCodecParameters* codecPar = avVideoStream->codecpar;
                    if (codecPar->color_primaries != AVCOL_PRI_UNSPECIFIED)
                    {
                        if (const char* name = av_color_primaries_name(codecPar->color_primaries))
                        {
                            _tags["Color Primaries"] = name;
                        }
                    }
                    if (codecPar->color_trc != AVCOL_TRC_UNSPECIFIED)
                    {
                        if (const char* name = av_color_transfer_name(codecPar->color_trc))
                        {
                            _tags["Color Transfer"] = name;
                        }
                    }
                    if (codecPar->color_space != AVCOL_SPC_UNSPECIFIED)
                    {
                        if (const char* name = av_color_space_name(codecPar->color_space))
                        {
                            _tags["Color Matrix"] = name;
                        }
                    }
                    {
                        _source.codec =
                            avcodec_get_name(_avCodecContext[_avStream]->codec_id);
                        if (const char* name = av_get_pix_fmt_name(_avInputPixelFormat))
                        {
                            _source.pixelFormat = name;
                        }
                    }
                }
            }
            catch (...)
            {
                _close();
                throw;
            }
        }

        ReadVideo::~ReadVideo()
        {
            _close();
        }

        void ReadVideo::_close()
        {
            _stopPacketThread();
            if (_swsContext)
            {
                sws_freeContext(_swsContext);
                _swsContext = nullptr;
            }
            if (_avFrame2)
            {
                av_frame_free(&_avFrame2);
            }
            if (_avFrame)
            {
                av_frame_free(&_avFrame);
            }
            if (_swFrame)
            {
                av_frame_free(&_swFrame);
            }
            for (auto i : _avCodecContext)
            {
                avcodec_free_context(&i.second);
            }
            for (auto i : _avCodecParameters)
            {
                avcodec_parameters_free(&i.second);
            }
            if (_avFormatContext)
            {
                avformat_close_input(&_avFormatContext);
            }
            if (_avIOContext)
            {
                av_freep(&_avIOContext->buffer);
                avio_context_free(&_avIOContext);
            }
            if (_hwDeviceContext)
            {
                av_buffer_unref(&_hwDeviceContext);
            }
        }

        bool ReadVideo::isValid() const
        {
            return _avStream != -1;
        }

        const ftk::ImageInfo& ReadVideo::getInfo() const
        {
            return _info;
        }

        const OTIO_NS::TimeRange& ReadVideo::getTimeRange() const
        {
            return _timeRange;
        }

        const VideoSourceInfo& ReadVideo::getSource() const
    {
        return _source;
    }

    const ftk::ImageTags& ReadVideo::getTags() const
        {
            return _tags;
        }

        namespace
        {
            std::string pixelFormatName(AVPixelFormat value)
            {
                const char* name = av_get_pix_fmt_name(value);
                return name ? name : "unknown";
            }

        }

        void ReadVideo::start()
        {
            if (_avStream != -1)
            {
                _avFrame = av_frame_alloc();
                if (!_avFrame)
                {
                    throw std::runtime_error(
                        ftk::Format("Cannot allocate frame: \"{0}\"").
                        arg(_fileName));
                }

                if (_hwAccel)
                {
                    // Hardware frames are downloaded into this one. Whether
                    // they need the scaler is only known when the first
                    // arrives (the download format, or the decoder's
                    // software-fallback format), so it is created in
                    // _copy() if at all.
                    _swFrame = av_frame_alloc();
                    if (!_swFrame)
                    {
                        throw std::runtime_error(
                            ftk::Format("Cannot allocate frame: \"{0}\"").
                            arg(_fileName));
                    }
                }
                if (!canCopy(_avInputPixelFormat, _avOutputPixelFormat))
                {
                    _initFrame2();
                    if (!_hwAccel)
                    {
                        _initSws(_avInputPixelFormat);
                    }
                }

                _startPacketThread();
            }
        }

        namespace
        {
            //! How many packets are read ahead: one is being decoded while
            //! the next is read, and a couple more cover a slow read.
            constexpr size_t packetsAhead = 4;
        }

        void ReadVideo::_startPacketThread()
        {
            _packets.thread = std::thread(
                [this]
                {
                    ftk::setCurrentThreadName("tl:ffmpeg-packets");
                    while (true)
                    {
                        std::unique_lock<std::mutex> lock(_packets.mutex);
                        _packets.cv.wait(
                            lock,
                            [this]
                            {
                                return
                                    _packets.stop ||
                                    (!_packets.pause &&
                                        0 == _packets.error &&
                                        _packets.packets.size() < packetsAhead);
                            });
                        if (_packets.stop)
                        {
                            break;
                        }
                        _packets.idle = false;
                        lock.unlock();
                        AVPacket* packet = av_packet_alloc();
                        const int r = av_read_frame(_avFormatContext, packet);
                        lock.lock();
                        _packets.idle = true;
                        if (_packets.pause)
                        {
                            // A seek came in during the read: the packet is
                            // from before it.
                            av_packet_free(&packet);
                        }
                        else if (r < 0)
                        {
                            av_packet_free(&packet);
                            _packets.error = r;
                        }
                        else
                        {
                            _packets.packets.push_back(packet);
                        }
                        _packets.cv.notify_all();
                    }
                });
        }

        void ReadVideo::_stopPacketThread()
        {
            if (_packets.thread.joinable())
            {
                {
                    std::unique_lock<std::mutex> lock(_packets.mutex);
                    _packets.stop = true;
                }
                _packets.cv.notify_all();
                _packets.thread.join();
            }
            for (AVPacket* packet : _packets.packets)
            {
                av_packet_free(&packet);
            }
            _packets.packets.clear();
        }

        int ReadVideo::_readPacket(AVPacket* out)
        {
            int r = 0;
            std::unique_lock<std::mutex> lock(_packets.mutex);
            _packets.cv.wait(
                lock,
                [this]
                {
                    return !_packets.packets.empty() || 0 != _packets.error;
                });
            if (!_packets.packets.empty())
            {
                AVPacket* packet = _packets.packets.front();
                _packets.packets.pop_front();
                av_packet_move_ref(out, packet);
                av_packet_free(&packet);
                lock.unlock();
                _packets.cv.notify_all();
            }
            else
            {
                r = _packets.error;
            }
            return r;
        }

        bool ReadVideo::_skipPackets(int64_t timestamp)
        {
            bool out = false;
            {
                std::unique_lock<std::mutex> lock(_packets.mutex);
                if (0 != _packets.error)
                {
                    return false;
                }
                while (!_packets.packets.empty())
                {
                    AVPacket* packet = _packets.packets.front();
                    const int64_t ts = packet->pts != AV_NOPTS_VALUE ? packet->pts : packet->dts;
                    if (ts >= timestamp)
                    {
                        out = true;
                        break;
                    }
                    _packets.packets.pop_front();
                    av_packet_free(&packet);
                }
            }
            // The thread may have been waiting on a full queue: there is
            // room now, and the reader is about to wait for what it reads.
            _packets.cv.notify_all();
            return out;
        }

        int ReadVideo::_seekFormat(int64_t timestamp, int flags)
        {
            {
                std::unique_lock<std::mutex> lock(_packets.mutex);
                _packets.pause = true;
                _packets.cv.wait(lock, [this] { return _packets.idle; });
                for (AVPacket* packet : _packets.packets)
                {
                    av_packet_free(&packet);
                }
                _packets.packets.clear();
                _packets.error = 0;
            }
            const int r = av_seek_frame(_avFormatContext, _avStream, timestamp, flags);
            {
                std::unique_lock<std::mutex> lock(_packets.mutex);
                _packets.pause = false;
            }
            _packets.cv.notify_all();
            return r;
        }

        void ReadVideo::_log(const std::string& message, ftk::LogType type) const
        {
            if (auto logSystem = _logSystem.lock())
            {
                logSystem->print("tl::ffmpeg::ReadVideo", message, type);
            }
        }

        AVPixelFormat ReadVideo::_getHwFormat(
            AVCodecContext* context,
            const AVPixelFormat* formats)
        {
            auto self = static_cast<ReadVideo*>(context->opaque);
            for (const AVPixelFormat* p = formats; *p != AV_PIX_FMT_NONE; ++p)
            {
                if (*p == self->_hwPixelFormat)
                {
                    return *p;
                }
            }
            // The hardware format was not offered for this stream; let the decoder
            // fall back to a software format. _decode() detects this per frame (the
            // frame format will not match _hwPixelFormat, so no download happens)
            // and _copy() builds the scaler from whatever format actually arrives.
            return formats[0];
        }

        namespace
        {
#if defined(__APPLE__)
            //! Whether this machine has a hardware decoder for a codec.
            //!
            //! VideoToolbox does not refuse a codec it has no hardware for:
            //! it decodes in software and hands the frames back the same
            //! way, which is slower than FFmpeg's own decoder on every core
            //! -- an Intel Mac without an HEVC decoder played 1080p at two
            //! thirds speed through it. A codec not listed here is not
            //! asked about, and is left to VideoToolbox as before.
            bool hasVideoToolboxDecoder(AVCodecID id)
            {
                CMVideoCodecType type = 0;
                switch (id)
                {
                case AV_CODEC_ID_H264: type = kCMVideoCodecType_H264; break;
                case AV_CODEC_ID_HEVC: type = kCMVideoCodecType_HEVC; break;
                // The four character codes themselves, since the names for
                // these are newer than the oldest system supported.
                case AV_CODEC_ID_VP9: type = 'vp09'; break;
                case AV_CODEC_ID_AV1: type = 'av01'; break;
                default: return true;
                }
                return VTIsHardwareDecodeSupported(type);
            }
#endif // __APPLE__
        }

        void ReadVideo::_initHwAccel(const AVCodec* codec)
        {
            // The hardware path outputs limited-range YUV, so full-range
            // sources stay on the software decoder. Any chroma subsampling
            // the hardware decodes is taken: its frames are copied into the
            // image the software decoder makes (see _copy()), and the first
            // frame is checked for the source's chroma, depth and alpha,
            // falling back to software when the hardware returned less. (A
            // 4:2:2 gate stood here until the cause of the difference it
            // guarded was found: hardware frames were converted to 4:2:0.)
            // Without a software decoder to fall back to, the reader
            // refuses the file after this and says why, so there is no
            // software decoding to announce.
            const bool software = !isHardwareOnly(_avCodecDefault);
            if (AVCOL_RANGE_JPEG == _avCodecParameters[_avStream]->color_range)
            {
                if (software)
                {
                    _log(
                        ftk::Format("Hardware decoding skipped for a full-range source; using software decoding: \"{0}\"").arg(_fileName),
                        ftk::LogType::Warning);
                }
                return;
            }
#if defined(__APPLE__)
            if (!hasVideoToolboxDecoder(codec->id))
            {
                if (software)
                {
                    _log(
                        ftk::Format("This machine has no hardware decoder for the codec \"{0}\"; using software decoding").
                        arg(codec->name ? codec->name : "?"),
                        ftk::LogType::Warning);
                }
                return;
            }
#endif // __APPLE__
            // The platform's native API is preferred, but the machine
            // decides: every configuration the codec offers is tried
            // until a device actually creates. A fixed choice broke on
            // hardware that speaks a different API -- VAAPI means
            // nothing to an NVIDIA driver, whose path is CUDA (#833).
#if defined(__APPLE__)
            const AVHWDeviceType preferred = AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
#elif defined(_WIN32)
            const AVHWDeviceType preferred = AV_HWDEVICE_TYPE_D3D11VA;
#else
            const AVHWDeviceType preferred = AV_HWDEVICE_TYPE_VAAPI;
#endif
            AVPixelFormat hwFormat = AV_PIX_FMT_NONE;
            AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
            AVBufferRef* device = nullptr;
            bool codecHasHw = false;
            std::string tried;
            for (int pass = 0; pass < 2 && !device; ++pass)
            {
                for (int i = 0; !device; ++i)
                {
                    const AVCodecHWConfig* config =
                        avcodec_get_hw_config(codec, i);
                    if (!config)
                    {
                        break;
                    }
                    if (!(config->methods &
                        AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX))
                    {
                        continue;
                    }
                    codecHasHw = true;
                    const bool isPreferred =
                        config->device_type == preferred;
                    if (0 == pass ? !isPreferred : isPreferred)
                    {
                        continue;
                    }
                    if (av_hwdevice_ctx_create(
                        &device, config->device_type,
                        nullptr, nullptr, 0) >= 0)
                    {
                        hwFormat = config->pix_fmt;
                        type = config->device_type;
                    }
                    else
                    {
                        if (!tried.empty())
                        {
                            tried += ", ";
                        }
                        tried += av_hwdevice_get_type_name(
                            config->device_type);
                    }
                }
            }
            if (!codecHasHw)
            {
                // This codec has no hardware support in this build of
                // FFmpeg; stay on the software path.
                _log(
                    ftk::Format("Hardware decoding is not available for the codec \"{0}\"; using software decoding").
                    arg(codec->name ? codec->name : "?"),
                    ftk::LogType::Warning);
                return;
            }
            if (!device)
            {
                _log(
                    ftk::Format("Cannot create a hardware decoding device ({0}); using software decoding").
                    arg(tried),
                    ftk::LogType::Warning);
                return;
            }
            _hwDeviceContext = device;
            _hwPixelFormat = hwFormat;
            _hwAccel = true;
            _avCodecContext[_avStream]->hw_device_ctx = av_buffer_ref(_hwDeviceContext);
            _avCodecContext[_avStream]->opaque = this;
            _avCodecContext[_avStream]->get_format = _getHwFormat;
            _log(
                ftk::Format("Hardware decoding enabled ({0}) for the codec \"{1}\"").
                arg(av_hwdevice_get_type_name(type)).
                arg(codec->name ? codec->name : "?"));
        }

        void ReadVideo::_initFrame2()
        {
            _avFrame2 = av_frame_alloc();
            if (!_avFrame2)
            {
                throw std::runtime_error(
                    ftk::Format("Cannot allocate frame: \"{0}\"").
                    arg(_fileName));
            }
            _avFrame2->format = _avOutputPixelFormat;
            _avFrame2->width = _info.size.w;
            _avFrame2->height = _info.size.h;
            _avFrame2->buf[0] = av_buffer_alloc(_info.getByteCount());
        }

        void ReadVideo::_initSws(AVPixelFormat srcFormat)
        {
            // May be a rebuild: see _copy().
            if (_swsContext)
            {
                sws_freeContext(_swsContext);
                _swsContext = nullptr;
            }
            _swsContext = sws_alloc_context();
            if (!_swsContext)
            {
                throw std::runtime_error(ftk::Format("Cannot allocate context: \"{0}\"").arg(_fileName));
            }
            av_opt_set_defaults(_swsContext);
            int r = av_opt_set_int(_swsContext, "srcw", _avCodecParameters[_avStream]->width, AV_OPT_SEARCH_CHILDREN);
            r = av_opt_set_int(_swsContext, "srch", _avCodecParameters[_avStream]->height, AV_OPT_SEARCH_CHILDREN);
            r = av_opt_set_int(_swsContext, "src_format", srcFormat, AV_OPT_SEARCH_CHILDREN);
            r = av_opt_set_int(_swsContext, "dstw", _avCodecParameters[_avStream]->width, AV_OPT_SEARCH_CHILDREN);
            r = av_opt_set_int(_swsContext, "dsth", _avCodecParameters[_avStream]->height, AV_OPT_SEARCH_CHILDREN);
            r = av_opt_set_int(_swsContext, "dst_format", _avOutputPixelFormat, AV_OPT_SEARCH_CHILDREN);
            r = av_opt_set_int(_swsContext, "sws_flags", swsReadFlags, AV_OPT_SEARCH_CHILDREN);
            r = av_opt_set_int(_swsContext, "threads", _options.threadCount, AV_OPT_SEARCH_CHILDREN);
            r = sws_init_context(_swsContext, nullptr, nullptr);
            if (r < 0)
            {
                throw std::runtime_error(ftk::Format("Cannot initialize sws context: \"{0}\"").arg(_fileName));
            }
            // The matrix and range to convert with, which the scaler does not
            // take from the frames: left to its defaults it converts with
            // BT.601 whatever the stream says, and moves full range JPEG
            // pixels to video range, which the image is then not described
            // as. A YUV output keeps the source's range; RGB is full.
            const AVPixFmtDescriptor* srcDesc = av_pix_fmt_desc_get(srcFormat);
            const AVPixFmtDescriptor* dstDesc = av_pix_fmt_desc_get(_avOutputPixelFormat);
            const bool srcFull =
                (srcDesc && (srcDesc->flags & AV_PIX_FMT_FLAG_RGB)) ||
                isFullRange(_avCodecContext[_avStream]->color_range, srcFormat);
            const bool dstFull =
                (dstDesc && (dstDesc->flags & AV_PIX_FMT_FLAG_RGB)) ||
                srcFull;
            const int* coefficients = sws_getCoefficients(
                toSwsColorspace(_info.yuvCoefficients));
            sws_setColorspaceDetails(
                _swsContext,
                coefficients,
                srcFull ? 1 : 0,
                coefficients,
                dstFull ? 1 : 0,
                0,
                1 << 16,
                1 << 16);
            // Recorded once there is a scaler it describes.
            _swsInputPixelFormat = srcFormat;
        }

        int ReadVideo::_openCodec(const AVCodec* codec, bool hwAccel)
        {
            if (_avCodecContext[_avStream])
            {
                avcodec_free_context(&_avCodecContext[_avStream]);
            }
            _avCodecContext[_avStream] = avcodec_alloc_context3(codec);
            if (!_avCodecContext[_avStream])
            {
                throw std::runtime_error(
                    ftk::Format("Cannot allocate context: \"{0}\"").
                    arg(_fileName));
            }
            int r = avcodec_parameters_to_context(
                _avCodecContext[_avStream],
                _avCodecParameters[_avStream]);
            if (r < 0)
            {
                throw std::runtime_error(
                    ftk::Format("{0}: \"{1}\"").
                    arg(getErrorLabel(r)).
                    arg(_fileName));
            }
            _avCodecContext[_avStream]->thread_count = _options.threadCount;
            // Frames of an intra only codec -- DNxHR, ProRes, MJPEG -- are
            // decoded across the threads a slice at a time rather than a
            // frame to a thread. Frame threading keeps a frame in flight for
            // every thread, and a seek throws them away and waits for them
            // again: a reader behind the playhead seeks on every request,
            // and an 8K DNxHR 444 movie decoded at ten frames a second
            // rather than twenty. Slices have nothing in flight, and scale
            // further: decoding that movie from memory, 67 frames a second
            // on 32 threads, where frames peaked at 47 on 16.
            const AVCodecDescriptor* descriptor = avcodec_descriptor_get(codec->id);
            const bool intraOnly = descriptor && (descriptor->props & AV_CODEC_PROP_INTRA_ONLY);
            _intraOnly = intraOnly;
            const bool sliceThreads = codec->capabilities & AV_CODEC_CAP_SLICE_THREADS;
            _avCodecContext[_avStream]->thread_type = intraOnly && sliceThreads ?
                FF_THREAD_SLICE :
                FF_THREAD_FRAME;
            if (hwAccel)
            {
                // Attempt hardware decode. On any failure this is a no-op
                // and decoding stays on the software path.
                _initHwAccel(codec);
            }
            return avcodec_open2(_avCodecContext[_avStream], codec, 0);
        }

        bool ReadVideo::_hwFallback(const OTIO_NS::RationalTime& currentTime)
        {
            // A hardware-only decoder can open and still fail at the first
            // frame -- the device exists, but the codec it was asked for does
            // not, an AV1 file on a GPU without AV1 decode -- and hardware on
            // the default decoder can prove unfaithful at the first frame.
            // Either way: rebuild on the software default and pick up from
            // the current time.
            if (!_avCodecDefault ||
                (_avCodec == _avCodecDefault && !_hwAccel))
            {
                return false;
            }
            _log(
                ftk::Format("Hardware decoding failed for the codec \"{0}\"; using software decoding: \"{1}\"").
                arg(_avCodec->name ? _avCodec->name : "?").
                arg(_fileName),
                ftk::LogType::Warning);
            if (_hwDeviceContext)
            {
                av_buffer_unref(&_hwDeviceContext);
            }
            _hwAccel = false;
            _hwPixelFormat = AV_PIX_FMT_NONE;
            _avCodec = _avCodecDefault;
            if (_openCodec(_avCodec, false) < 0)
            {
                return false;
            }
            // A new decoder has no position to walk on from.
            _decodedTime.reset();
            seek(currentTime);
            return true;
        }

        void ReadVideo::seek(const OTIO_NS::RationalTime& time)
        {
            if (_avStream != -1)
            {
                AVStream* stream = _avFormatContext->streams[_avStream];
                const auto toStream = [this, stream](const OTIO_NS::RationalTime& value)
                {
                    return av_rescale_q(
                        value.value() - _timeRange.start_time().value(),
                        swap(_avSpeed),
                        stream->time_base);
                };
                const int64_t timestamp = toStream(time);

                // A frame a little ahead of the last one decoded is reached
                // by decoding on from here, when the keyframe a seek would
                // land on is behind where the decoder already is: the walk
                // from the keyframe is the cost of a seek in a long GOP
                // movie, and scrubbing forward paid it for every frame.
                // Decided from the index when the container has one, and
                // for a short way ahead when it does not.
                bool walk = false;
                if (_decodedTime.has_value() && !_eof && time > _decodedTime.value())
                {
                    const int i = av_index_search_timestamp(
                        stream,
                        timestamp,
                        AVSEEK_FLAG_BACKWARD);
                    if (i >= 0)
                    {
                        const AVIndexEntry* entry = avformat_index_get_entry(stream, i);
                        walk = entry && entry->timestamp <= toStream(_decodedTime.value());
                    }
                    else
                    {
                        walk = (time - _decodedTime.value()).rescaled_to(1.0).value() <= 1.0;
                    }
                }
                _seekKeyframe = -1;
                // A frame a few ahead with every frame a keyframe -- an
                // intra codec, DNxHR or ProRes, where the walk above is
                // never taken -- is reached by dropping the packets queued
                // before it: a seek threw the packets read ahead away, to
                // read them again, and flushed the decoder, so a reader a
                // little behind the playhead that skipped a frame or two
                // (see IVideoRead::setPlayhead) paid two frames' worth for
                // each skip and fell further behind with every one. An 8K
                // movie asked for at 48 frames a second from a reader good
                // for 43 showed 11. A frame a little past the packets
                // queued is read on to the same way, the packets before it
                // demuxed and discarded (see process()): a seek costs more
                // than a few packets read in order. Further than that, a
                // seek.
                if (!walk &&
                    _intraOnly &&
                    !_measuredRate &&
                    _decodedTime.has_value() &&
                    !_eof &&
                    time > _decodedTime.value())
                {
                    const bool queued = _skipPackets(timestamp);
                    walk = queued ||
                        (time - _decodedTime.value()).rescaled_to(_timeRange.duration().rate()).value() <=
                            2.0 * packetsAhead;
                }
                if (!walk)
                {
                    _packetTimes.clear();
                    int64_t seekTimestamp = timestamp;
                    _seekKeyframe = av_index_search_timestamp(
                        stream,
                        timestamp,
                        AVSEEK_FLAG_BACKWARD);
                    if (_measuredRate)
                    {
                        // A keyframe earlier: see _measuredRate.
                        for (int i = _seekKeyframe - 1; i >= 0; --i)
                        {
                            const AVIndexEntry* entry = avformat_index_get_entry(stream, i);
                            if (entry && (entry->flags & AVINDEX_KEYFRAME))
                            {
                                _seekKeyframe = i;
                                seekTimestamp = entry->timestamp;
                                break;
                            }
                        }
                    }
                    avcodec_flush_buffers(_avCodecContext[_avStream]);
                    const int seekError = _seekFormat(seekTimestamp, AVSEEK_FLAG_BACKWARD);
                    if (seekError < 0)
                    {
                        _setError(seekError);
                    }
                    _decodedTime.reset();
                }
            }
            _buffer.clear();
            _eof = false;
            _seekWalk = true;
            _seekTarget = time;
        }

        size_t ReadVideo::getErrorCount() const
        {
            return _errorCount;
        }

        const std::string& ReadVideo::getErrorString() const
        {
            return _errorString;
        }

        void ReadVideo::_setError(int error)
        {
            ++_errorCount;
            if (_errorString.empty())
            {
                _errorString = getErrorLabel(error);
            }
        }

        bool ReadVideo::process(const OTIO_NS::RationalTime& currentTime)
        {
            bool out = false;
            if (_avStream != -1 &&
                _buffer.size() < _options.videoBufferSize)
            {
                Packet packet;
                int decoding = 0;
                while (0 == decoding)
                {
                    if (!_eof)
                    {
                        decoding = _readPacket(packet.p);
                        if (AVERROR_EOF == decoding)
                        {
                            _eof = true;
                            decoding = 0;
                        }
                        else if (decoding < 0)
                        {
                            _setError(decoding);
                            break;
                        }
                    }
                    if ((_eof && _avStream != -1) || (_avStream == packet.p->stream_index))
                    {
                        if (_measuredRate && !_eof && packet.p->pts != AV_NOPTS_VALUE)
                        {
                            _packetTimes.insert(packet.p->pts);
                            while (_packetTimes.size() > 64)
                            {
                                _packetTimes.erase(_packetTimes.begin());
                            }
                        }
                        // The walk from the keyframe to the frame a seek
                        // asked for: a frame before it that nothing refers
                        // to is never shown and never needed, so the
                        // decoder is told to drop it. Half the frames of a
                        // typical encode, and with the hardware decoding
                        // one frame at a time each one is a fixed wait.
                        if (_seekWalk && !_eof)
                        {
                            const int64_t ts = packet.p->pts != AV_NOPTS_VALUE ?
                                packet.p->pts : packet.p->dts;
                            const OTIO_NS::RationalTime packetTime(
                                _timeRange.start_time().value() +
                                av_rescale_q(
                                    ts,
                                    _avFormatContext->streams[_avStream]->time_base,
                                    swap(_avSpeed)),
                                _timeRange.duration().rate());
                            // A tick of slack where a frame can be moved a
                            // tick on (see _measuredRate): judged by its
                            // own time it was before the one asked for, and
                            // was skipped when it was the one.
                            const OTIO_NS::RationalTime slack(
                                _measuredRate ? 1.0 : 0.0,
                                _timeRange.duration().rate());
                            if (packetTime + slack < _seekTarget)
                            {
                                // With every frame a keyframe nothing ahead
                                // depends on these, so they are not decoded
                                // at all; elsewhere the frames nothing refers
                                // to are the ones that can go.
                                _avCodecContext[_avStream]->skip_frame = _intraOnly ?
                                    AVDISCARD_ALL :
                                    AVDISCARD_NONREF;
                            }
                            else
                            {
                                _avCodecContext[_avStream]->skip_frame = AVDISCARD_DEFAULT;
                                _seekWalk = false;
                            }
                        }
                        decoding = avcodec_send_packet(
                            _avCodecContext[_avStream],
                            _eof ? nullptr : packet.p);
                        if (AVERROR_EOF == decoding)
                        {
                            decoding = 0;
                        }
                        else if (decoding < 0)
                        {
                            if (_hwFallback(currentTime))
                            {
                                decoding = 0;
                                continue;
                            }
                            _setError(decoding);
                            break;
                        }
                        decoding = _decode(currentTime);
                        if (AVERROR(EAGAIN) == decoding)
                        {
                            decoding = 0;
                        }
                        else if (AVERROR_EOF == decoding)
                        {
                            break;
                        }
                        else if (decoding < 0)
                        {
                            if (_hwFallback(currentTime))
                            {
                                decoding = 0;
                                continue;
                            }
                            _setError(decoding);
                            break;
                        }
                        else if (1 == decoding)
                        {
                            out = true;
                            break;
                        }
                    }
                    if (packet.p->buf)
                    {
                        av_packet_unref(packet.p);
                    }
                }
                if (packet.p->buf)
                {
                    av_packet_unref(packet.p);
                }
            }
            return out;
        }

        bool ReadVideo::isBufferEmpty() const
        {
            return _buffer.empty();
        }

        void ReadVideo::dropBefore(const OTIO_NS::RationalTime& time)
        {
            while (!_buffer.empty() && _buffer.front().first < time)
            {
                _buffer.pop_front();
            }
        }

        std::shared_ptr<ftk::Image> ReadVideo::frameFor(const OTIO_NS::RationalTime& time)
        {
            dropBefore(time);
            return !_buffer.empty() ? _buffer.front().second : nullptr;
        }

        OTIO_NS::RationalTime ReadVideo::_frameTime(int64_t timestamp) const
        {
            const AVRational timeBase = _avFormatContext->streams[_avStream]->time_base;
            int64_t tick = av_rescale_q(timestamp, timeBase, swap(_avSpeed));
            if (_measuredRate)
            {
                // The later of two frames on one tick goes to the next;
                // see _measuredRate.
                auto i = _packetTimes.lower_bound(timestamp);
                if (i != _packetTimes.begin())
                {
                    --i;
                    if (av_rescale_q(*i, timeBase, swap(_avSpeed)) == tick)
                    {
                        ++tick;
                    }
                }
            }
            return OTIO_NS::RationalTime(
                _timeRange.start_time().value() + tick,
                _timeRange.duration().rate());
        }

        int ReadVideo::_decode(const OTIO_NS::RationalTime& currentTime)
        {
            int out = 0;
            while (0 == out)
            {
                out = avcodec_receive_frame(_avCodecContext[_avStream], _avFrame);
                if (out < 0)
                {
                    return out;
                }
                AVFrame* frame = _avFrame;
                bool textureFrame = false;
#if defined(__APPLE__)
                if (_hwAccel && _textures &&
                    _avFrame->format == _hwPixelFormat &&
                    AV_PIX_FMT_NV12 == _avOutputPixelFormat)
                {
                    // The frame stays where VideoToolbox put it: an NV12
                    // pixel buffer the renderer makes its textures over.
                    // Anything else the hardware hands back is downloaded
                    // as before.
                    CVPixelBufferRef pixelBuffer =
                        reinterpret_cast<CVPixelBufferRef>(_avFrame->data[3]);
                    const OSType format = pixelBuffer ?
                        CVPixelBufferGetPixelFormatType(pixelBuffer) : 0;
                    textureFrame =
                        kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange == format ||
                        kCVPixelFormatType_420YpCbCr8BiPlanarFullRange == format;
                }
#endif // __APPLE__
                if (_hwAccel && !textureFrame && _avFrame->format == _hwPixelFormat)
                {
                    // Download the hardware surface to a CPU frame (NV12/P010).
                    av_frame_unref(_swFrame);
                    if (av_hwframe_transfer_data(_swFrame, _avFrame, 0) < 0)
                    {
                        _log(
                            ftk::Format("Cannot download a hardware frame; skipping: \"{0}\"").
                            arg(_fileName),
                            ftk::LogType::Warning);
                        continue;
                    }
                    av_frame_copy_props(_swFrame, _avFrame);
                    frame = _swFrame;
                    if (!_hwLogged)
                    {
                        // The first downloaded frame says whether the hardware
                        // kept its word: coarser chroma, fewer bits, or no
                        // alpha where the source has it means the image would
                        // not be a faithful match, and the error puts decoding
                        // back on software.
                        const AVPixFmtDescriptor* srcDesc =
                            av_pix_fmt_desc_get(_avInputPixelFormat);
                        const AVPixFmtDescriptor* dlDesc = av_pix_fmt_desc_get(
                            static_cast<AVPixelFormat>(_swFrame->format));
                        if (srcDesc && dlDesc &&
                            (dlDesc->log2_chroma_w > srcDesc->log2_chroma_w ||
                             dlDesc->log2_chroma_h > srcDesc->log2_chroma_h ||
                             dlDesc->comp[0].depth < srcDesc->comp[0].depth ||
                             ((srcDesc->flags & AV_PIX_FMT_FLAG_ALPHA) &&
                              !(dlDesc->flags & AV_PIX_FMT_FLAG_ALPHA))))
                        {
                            _log(
                                ftk::Format("Hardware decoding is coarser than the source ({0} for {1}); using software decoding: \"{2}\"").
                                arg(dlDesc->name ? dlDesc->name : "?").
                                arg(srcDesc->name ? srcDesc->name : "?").
                                arg(_fileName),
                                ftk::LogType::Warning);
                            return AVERROR_EXTERNAL;
                        }
                        // Confirms frames are really decoding on the hardware, as
                        // opposed to the device being attached but the decoder
                        // having fallen back to software (see _getHwFormat()).
                        // The downloaded format is named: it is the first
                        // question when a hardware decode looks wrong.
                        _log(ftk::Format("Hardware decoding is active ({0}): \"{1}\"").
                            arg(dlDesc && dlDesc->name ? dlDesc->name : "?").
                            arg(_fileName));
                        _hwLogged = true;
                    }
                }
                const int64_t timestamp = _avFrame->pts != AV_NOPTS_VALUE ? _avFrame->pts : _avFrame->pkt_dts;

                const OTIO_NS::RationalTime time = _frameTime(timestamp);

                // The first frame after a seek, past the frame asked for:
                // the seek landed on a keyframe shown after it, and is
                // made again from the keyframe before (see _seekKeyframe).
                if (_seekKeyframe >= 0)
                {
                    const int keyframe = _seekKeyframe;
                    _seekKeyframe = -1;
                    if (time > _seekTarget)
                    {
                        AVStream* stream = _avFormatContext->streams[_avStream];
                        for (int i = keyframe - 1; i >= 0; --i)
                        {
                            const AVIndexEntry* entry = avformat_index_get_entry(stream, i);
                            if (entry && (entry->flags & AVINDEX_KEYFRAME))
                            {
                                avcodec_flush_buffers(_avCodecContext[_avStream]);
                                if (_seekFormat(entry->timestamp, AVSEEK_FLAG_BACKWARD) >= 0)
                                {
                                    _seekKeyframe = i;
                                    _seekWalk = true;
                                    _eof = false;
                                    _decodedTime.reset();
                                    _packetTimes.clear();
                                    return AVERROR(EAGAIN);
                                }
                                break;
                            }
                        }
                    }
                }
                _decodedTime = time;

                if (time >= currentTime)
                {
                    std::shared_ptr<ftk::Image> image;
#if defined(__APPLE__)
                    if (textureFrame)
                    {
                        // The image is the pixel buffer, kept for as long
                        // as the image is: the decoder's pool hands out
                        // another in the meantime.
                        CVPixelBufferRef pixelBuffer =
                            reinterpret_cast<CVPixelBufferRef>(_avFrame->data[3]);
                        CVPixelBufferRetain(pixelBuffer);
                        image = ftk::Image::create(
                            _info,
                            reinterpret_cast<uintptr_t>(pixelBuffer),
                            [](uintptr_t handle)
                            {
                                CVPixelBufferRelease(reinterpret_cast<CVPixelBufferRef>(handle));
                            });
                        if (!_hwLogged)
                        {
                            _log(ftk::Format("Hardware decoding is active (nv12, as textures): \"{0}\"").
                                arg(_fileName));
                            _hwLogged = true;
                        }
                    }
#endif // __APPLE__
                    if (!image)
                    {
                        image = ftk::Image::create(_info);
                    }
                    
                    auto tags = _tags;
                    AVDictionaryEntry* tag = nullptr;
                    while ((tag = av_dict_get(_avFrame->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
                    {
                        tags[tag->key] = tag->value;
                    }
                    HDRData hdrData;
                    toHDRData(_avFrame->side_data, _avFrame->nb_side_data, hdrData);
                    tags["hdr"] = nlohmann::json(hdrData).dump();
                    image->setTags(tags);

                    if (!textureFrame)
                    {
                        _copy(image, frame);
                    }
                    _buffer.push_back({ time, image });
                    out = 1;
                    break;
                }
            }
            return out;
        }

        void ReadVideo::_copy(const std::shared_ptr<ftk::Image>& image, AVFrame* frame)
        {
            const auto& info = image->getInfo();
            const std::size_t w = info.size.w;
            const std::size_t h = info.size.h;
            uint8_t* const data = image->getData();
            if (_hwAccel &&
                frame->format == _avOutputPixelFormat &&
                isSemiPlanar(_avOutputPixelFormat))
            {
                // Native semi-planar copy (NV12): luma plane, then the
                // interleaved chroma plane, straight into the ftk image. No colour
                // conversion -- the display shader does YUV->RGB. The sws fallback
                // below covers the rare case of an unexpected download format.
                const std::size_t bytes =
                    (AV_PIX_FMT_P010LE == _avOutputPixelFormat) ? 2 : 1;
                const uint8_t* const dataY = frame->data[0];
                const uint8_t* const dataUV = frame->data[1];
                const int linesizeY = frame->linesize[0];
                const int linesizeUV = frame->linesize[1];
                for (std::size_t i = 0; i < h; ++i)
                {
                    std::memcpy(data + w * bytes * i, dataY + linesizeY * i, w * bytes);
                }
                uint8_t* const dataOutUV = data + w * h * bytes;
                const std::size_t h2 = h / 2;
                for (std::size_t i = 0; i < h2; ++i)
                {
                    std::memcpy(dataOutUV + w * bytes * i, dataUV + linesizeUV * i, w * bytes);
                }
                return;
            }
            const AVPixelFormat frameFormat =
                static_cast<AVPixelFormat>(frame->format);
            if (canCopySemiPlanar(frameFormat, _avOutputPixelFormat))
            {
                // A hardware frame for an image the software decoder would
                // have made: 4:2:2 and 4:4:4, and 4:2:0 deeper than 8 bits.
                copySemiPlanar(frame, data, _avOutputPixelFormat, w, h);
                return;
            }
            // The format the stream declares is the format of its first
            // frame, and a file may change it partway through: a QuickTime
            // can carry ProRes 4444 and ProRes 422 frames in one stream, and
            // the 422 stretches decode to a format the header never
            // mentioned. So the arriving frame is asked rather than the
            // header, and the scaler is rebuilt whenever the answer changes.
            if (canCopy(frameFormat, _avOutputPixelFormat))
            {
                const uint8_t* const data0 = frame->data[0];
                const int linesize0 = frame->linesize[0];
                switch (frameFormat)
                {
                case AV_PIX_FMT_RGB24:
                    for (std::size_t i = 0; i < h; ++i)
                    {
                        std::memcpy(
                            data + w * 3 * i,
                            data0 + linesize0 * 3 * i,
                            w * 3);
                    }
                    break;
                case AV_PIX_FMT_GRAY8:
                    for (std::size_t i = 0; i < h; ++i)
                    {
                        std::memcpy(
                            data + w * i,
                            data0 + linesize0 * i,
                            w);
                    }
                    break;
                case AV_PIX_FMT_RGBA:
                    for (std::size_t i = 0; i < h; ++i)
                    {
                        std::memcpy(
                            data + w * 4 * i,
                            data0 + linesize0 * 4 * i,
                            w * 4);
                    }
                    break;
                default:
                    copyPlanes(frame, data, frameFormat, w, h);
                    break;
                }
            }
            else
            {
                if (!_swsContext || _swsInputPixelFormat != frameFormat)
                {
                    if (_swsContext)
                    {
                        _log(ftk::Format(
                            "The video format changed from \"{0}\" to \"{1}\": \"{2}\"").
                            arg(pixelFormatName(_swsInputPixelFormat)).
                            arg(pixelFormatName(frameFormat)).
                            arg(_fileName));
                    }
                    // Built here rather than in start() because the format
                    // that arrives is only known now: the hardware download
                    // format, a software-fallback format, or a change
                    // partway through the stream.
                    _initSws(frameFormat);
                }
                if (!_avFrame2)
                {
                    // The stream started in a format that could be copied
                    // straight out, so start() had no scaling to set up for.
                    _initFrame2();
                }
                av_image_fill_arrays(
                    _avFrame2->data,
                    _avFrame2->linesize,
                    data,
                    _avOutputPixelFormat,
                    w,
                    h,
                    1);
                // The matrix and range the image is described with. A YUV
                // output keeps both, so the scaler changes the layout and
                // nothing else: left to its defaults it converted full range
                // pixels to video range that were then drawn as full range,
                // and blacks and whites came out gray.
                frame->color_range = isFullRange(
                    _avCodecContext[_avStream]->color_range,
                    frameFormat) ?
                    AVCOL_RANGE_JPEG :
                    AVCOL_RANGE_MPEG;
                frame->colorspace = fromYUVCoefficients(_info.yuvCoefficients);
                const AVPixFmtDescriptor* outputDesc =
                    av_pix_fmt_desc_get(_avOutputPixelFormat);
                const bool outputRGB =
                    outputDesc && (outputDesc->flags & AV_PIX_FMT_FLAG_RGB);
                _avFrame2->color_range = outputRGB ?
                    AVCOL_RANGE_JPEG :
                    frame->color_range;
                _avFrame2->colorspace = outputRGB ?
                    AVCOL_SPC_RGB :
                    frame->colorspace;
                sws_scale_frame(_swsContext, _avFrame2, frame);

                // The scaler takes YUV to sixteen bit RGB with white at
                // 65280, 255 shifted up, where white is 65535: a ten bit or
                // deeper picture was read 0.39% low when the conversion was
                // done here, which is when it is asked for and whenever the
                // picture has alpha. And it widens alpha by shifting, so
                // opaque in ten bits was 65472. Both are brought up to
                // where they belong; see also the writer, which has the
                // same to undo on the way out.
                const AVPixFmtDescriptor* frameDesc = av_pix_fmt_desc_get(frameFormat);
                if (frameDesc &&
                    !(frameDesc->flags & AV_PIX_FMT_FLAG_RGB) &&
                    frameDesc->nb_components >= 3 &&
                    (AV_PIX_FMT_RGB48 == _avOutputPixelFormat ||
                        AV_PIX_FMT_RGBA64 == _avOutputPixelFormat))
                {
                    const bool alpha = AV_PIX_FMT_RGBA64 == _avOutputPixelFormat;
                    const size_t channels = alpha ? 4 : 3;
                    const int depth = frameDesc->comp[0].depth;
                    const uint32_t alphaMax = depth < 16 ?
                        (((1U << depth) - 1U) << (16 - depth)) :
                        65535U;
                    uint16_t* p = reinterpret_cast<uint16_t*>(data);
                    const size_t count = w * h;
                    for (size_t i = 0; i < count; ++i, p += channels)
                    {
                        for (size_t c = 0; c < 3; ++c)
                        {
                            const uint32_t v = (p[c] * 65535U + 32640U) / 65280U;
                            p[c] = static_cast<uint16_t>(std::min(v, 65535U));
                        }
                        if (alpha && alphaMax < 65535U)
                        {
                            const uint32_t v = (p[3] * 65535U + alphaMax / 2U) / alphaMax;
                            p[3] = static_cast<uint16_t>(std::min(v, 65535U));
                        }
                    }
                }
            }
        }
    }
}

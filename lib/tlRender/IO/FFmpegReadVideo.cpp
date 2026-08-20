// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/FFmpegReadPrivate.h>

#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>

extern "C"
{
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>

} // extern "C"

namespace tl
{
    namespace ffmpeg
    {
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
                if (!memory.empty())
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
                    !_avFormatContext ? fileName.c_str() : nullptr,
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
                std::string timecode = getTimecodeFromDataStream(_avFormatContext);
                if (_avStream != -1)
                {
                    //av_dump_format(_avFormatContext, _avStream, fileName.c_str(), 0);

                    auto avVideoStream = _avFormatContext->streams[_avStream];
                    auto avVideoCodecParameters = avVideoStream->codecpar;
                    auto avVideoCodec = avcodec_find_decoder(avVideoCodecParameters->codec_id);
                    if (!avVideoCodec)
                    {
                        throw std::runtime_error(
                            ftk::Format("No video codec found: \"{0}\"").
                            arg(fileName));
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
                    _avCodecContext[_avStream] = avcodec_alloc_context3(avVideoCodec);
                    if (!_avCodecContext[_avStream])
                    {
                        throw std::runtime_error(
                            ftk::Format("Cannot allocate context: \"{0}\"").
                            arg(fileName));
                    }
                    r = avcodec_parameters_to_context(_avCodecContext[_avStream], _avCodecParameters[_avStream]);
                    if (r < 0)
                    {
                        throw std::runtime_error(
                            ftk::Format("{0}: \"{1}\"").
                            arg(getErrorLabel(r)).
                            arg(fileName));
                    }
                    _avCodecContext[_avStream]->thread_count = options.threadCount;
                    _avCodecContext[_avStream]->thread_type = FF_THREAD_FRAME;
                    if (options.hwAccel)
                    {
                        // Attempt hardware decode. On any failure this is a no-op
                        // and decoding stays on the software path.
                        _initHwAccel(avVideoCodec);
                    }
                    r = avcodec_open2(_avCodecContext[_avStream], avVideoCodec, 0);
                    if (r < 0)
                    {
                        throw std::runtime_error(
                            ftk::Format("{0}: \"{1}\"").
                            arg(getErrorLabel(r)).
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
                            //! \todo Use the _info.layout.endian field instead of
                            //! converting endianness.
                            _avOutputPixelFormat = AV_PIX_FMT_YUV420P16LE;
                            _info.type = ftk::ImageType::YUV_420P_U16;
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
                            //! \todo Use the _info.layout.endian field instead of
                            //! converting endianness.
                            _avOutputPixelFormat = AV_PIX_FMT_YUV422P16LE;
                            _info.type = ftk::ImageType::YUV_422P_U16;
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
                            //! \todo Use the _info.layout.endian field instead of
                            //! converting endianness.
                            _avOutputPixelFormat = AV_PIX_FMT_YUV444P16LE;
                            _info.type = ftk::ImageType::YUV_444P_U16;
                        }
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
                    if (_hwAccel)
                    {
                        // Hardware frames download as NV12 (8-bit) or P010 (>8-bit).
                        // They are handed to the display shader as semi-planar YUV
                        // with no colour conversion -- the shader performs YUV->RGB
                        // exactly as for software-decoded YUV, so hardware and
                        // software decoding match, and the cache keeps the smaller
                        // YUV footprint. _avOutputPixelFormat doubles as the expected
                        // download format and the sws fallback target (see _copy()).
                        const AVPixFmtDescriptor* desc =
                            av_pix_fmt_desc_get(_avInputPixelFormat);
                        const bool gt8 = desc && desc->comp[0].depth > 8;
                        _avOutputPixelFormat = gt8 ? AV_PIX_FMT_P010LE : AV_PIX_FMT_NV12;
                        _info.type = gt8 ? ftk::ImageType::YUV_420SP_U16 : ftk::ImageType::YUV_420SP_U8;
                    }
                    if (_avCodecContext[_avStream]->color_range != AVCOL_RANGE_JPEG)
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
                    switch (_avCodecParameters[_avStream]->color_space)
                    {
                    case AVCOL_SPC_BT2020_NCL:
                        _info.yuvCoefficients = ftk::YUVCoefficients::BT2020;
                        break;
                    default: break;
                    }

                    _avSpeed = av_guess_frame_rate(_avFormatContext, avVideoStream, nullptr);
                    const double speed = av_q2d(_avSpeed);

                    std::size_t sequenceSize = 0;
                    if (avVideoStream->nb_frames > 0)
                    {
                        sequenceSize = avVideoStream->nb_frames;
                    }
                    else if (avVideoStream->duration != AV_NOPTS_VALUE)
                    {
                        sequenceSize = av_rescale_q(
                            avVideoStream->duration,
                            avVideoStream->time_base,
                            swap(avVideoStream->r_frame_rate));
                    }
                    else if (_avFormatContext->duration != AV_NOPTS_VALUE)
                    {
                        sequenceSize = av_rescale_q(
                            _avFormatContext->duration,
                            av_get_time_base_q(),
                            swap(avVideoStream->r_frame_rate));
                    }
        
                    ftk::ImageTags tags;
                    AVDictionaryEntry* tag = nullptr;
                    while ((tag = av_dict_get(_avFormatContext->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
                    {
                        const std::string key(tag->key);
                        const std::string value(tag->value);
                        tags[key] = value;
                        if (ftk::compare(
                            key,
                            "timecode",
                            ftk::CaseCompare::Insensitive))
                        {
                            timecode = value;
                        }
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
            bool canCopy(AVPixelFormat in, AVPixelFormat out)
            {
                return in == out &&
                    (AV_PIX_FMT_RGB24   == in ||
                     AV_PIX_FMT_GRAY8   == in ||
                     AV_PIX_FMT_RGBA    == in ||
                     AV_PIX_FMT_YUV420P == in);
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

                if (!canCopy(_avInputPixelFormat, _avOutputPixelFormat))
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

                    if (_hwAccel)
                    {
                        _swFrame = av_frame_alloc();
                        if (!_swFrame)
                        {
                            throw std::runtime_error(
                                ftk::Format("Cannot allocate frame: \"{0}\"").
                                arg(_fileName));
                        }
                        // The scaler is created lazily in _copy(), once the real
                        // source format is known (the hardware download format, or
                        // the decoder's software-fallback format).
                    }
                    else
                    {
                        _initSws(_avInputPixelFormat);
                    }
                }
            }
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

        void ReadVideo::_initHwAccel(const AVCodec* codec)
        {
            // The hardware path outputs 4:2:0 NV12/P010 with limited-range YUV
            // colour handling. For sources it would not reproduce faithfully --
            // full-range, or chroma subsampling other than 4:2:0 -- stay on the
            // software decoder so hardware decoding is always a faithful match
            // (it silently falls back rather than altering the image).
            const AVPixelFormat inputFormat =
                static_cast<AVPixelFormat>(_avCodecParameters[_avStream]->format);
            const AVPixFmtDescriptor* inputDesc = av_pix_fmt_desc_get(inputFormat);
            const bool is420 = inputDesc &&
                1 == inputDesc->log2_chroma_w && 1 == inputDesc->log2_chroma_h;
            if (AVCOL_RANGE_JPEG == _avCodecParameters[_avStream]->color_range)
            {
                _log(
                    ftk::Format("Hardware decoding skipped for a full-range source; using software decoding: \"{0}\"").arg(_fileName),
                    ftk::LogType::Warning);
                return;
            }
            if (!is420)
            {
                _log(
                    ftk::Format("Hardware decoding skipped for a non-4:2:0 source; using software decoding: \"{0}\"").arg(_fileName),
                    ftk::LogType::Warning);
                return;
            }
#if defined(__APPLE__)
            const AVHWDeviceType type = AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
#elif defined(_WIN32)
            const AVHWDeviceType type = AV_HWDEVICE_TYPE_D3D11VA;
#else
            const AVHWDeviceType type = AV_HWDEVICE_TYPE_VAAPI;
#endif
            // Find a hardware configuration for this codec and device type.
            AVPixelFormat hwFormat = AV_PIX_FMT_NONE;
            for (int i = 0; ; ++i)
            {
                const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
                if (!config)
                {
                    break;
                }
                if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) &&
                    config->device_type == type)
                {
                    hwFormat = config->pix_fmt;
                    break;
                }
            }
            if (AV_PIX_FMT_NONE == hwFormat)
            {
                // This codec has no hardware support here; stay on the software path.
                _log(
                    ftk::Format("Hardware decoding is not available for the codec \"{0}\"; using software decoding").
                    arg(codec->name ? codec->name : "?"),
                    ftk::LogType::Warning);
                return;
            }
            // Create the hardware device. On failure, stay on the software path.
            AVBufferRef* device = nullptr;
            if (av_hwdevice_ctx_create(&device, type, nullptr, nullptr, 0) < 0)
            {
                _log(
                    ftk::Format("Cannot create a hardware decoding device ({0}); using software decoding").
                    arg(av_hwdevice_get_type_name(type)),
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

        void ReadVideo::_initSws(AVPixelFormat srcFormat)
        {
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
            r = av_opt_set_int(_swsContext, "sws_flags", swsScaleFlags, AV_OPT_SEARCH_CHILDREN);
            r = av_opt_set_int(_swsContext, "threads", _options.threadCount, AV_OPT_SEARCH_CHILDREN);
            r = sws_init_context(_swsContext, nullptr, nullptr);
            if (r < 0)
            {
                throw std::runtime_error(ftk::Format("Cannot initialize sws context: \"{0}\"").arg(_fileName));
            }
        }

        void ReadVideo::seek(const OTIO_NS::RationalTime& time)
        {

            if (_avStream != -1)
            {
                avcodec_flush_buffers(_avCodecContext[_avStream]);

                const int seekError = av_seek_frame(
                    _avFormatContext,
                    _avStream,
                    av_rescale_q(
                        time.value() - _timeRange.start_time().value(),
                        swap(_avSpeed),
                        _avFormatContext->streams[_avStream]->time_base),
                    AVSEEK_FLAG_BACKWARD);
                if (seekError < 0)
                {
                    _setError(seekError);
                }
            }

            _buffer.clear();
            _eof = false;
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
                        decoding = av_read_frame(_avFormatContext, packet.p);
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
                        decoding = avcodec_send_packet(
                            _avCodecContext[_avStream],
                            _eof ? nullptr : packet.p);
                        if (AVERROR_EOF == decoding)
                        {
                            decoding = 0;
                        }
                        else if (decoding < 0)
                        {
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

        std::shared_ptr<ftk::Image> ReadVideo::popBuffer()
        {
            std::shared_ptr<ftk::Image> out;
            if (!_buffer.empty())
            {
                out = _buffer.front();
                _buffer.pop_front();
            }
            return out;
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
                if (_hwAccel && _avFrame->format == _hwPixelFormat)
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
                        // Confirms frames are really decoding on the hardware, as
                        // opposed to the device being attached but the decoder
                        // having fallen back to software (see _getHwFormat()).
                        _log(ftk::Format("Hardware decoding is active: \"{0}\"").arg(_fileName));
                        _hwLogged = true;
                    }
                }
                const int64_t timestamp = _avFrame->pts != AV_NOPTS_VALUE ? _avFrame->pts : _avFrame->pkt_dts;

                const OTIO_NS::RationalTime time(
                    _timeRange.start_time().value() +
                    av_rescale_q(
                        timestamp,
                        _avFormatContext->streams[_avStream]->time_base,
                        swap(_avFormatContext->streams[_avStream]->r_frame_rate)),
                    _timeRange.duration().rate());

                if (time >= currentTime)
                {
                    auto image = ftk::Image::create(_info);
                    
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

                    _copy(image, frame);
                    _buffer.push_back(image);
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
            if (_hwAccel && frame->format == _avOutputPixelFormat)
            {
                // Native semi-planar copy (NV12/P010): luma plane, then the
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
            if (canCopy(_avInputPixelFormat, _avOutputPixelFormat))
            {
                const uint8_t* const data0 = frame->data[0];
                const int linesize0 = frame->linesize[0];
                switch (_avInputPixelFormat)
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
                case AV_PIX_FMT_YUV420P:
                {
                    const std::size_t w2 = w / 2;
                    const std::size_t h2 = h / 2;
                    const uint8_t* const data1 = frame->data[1];
                    const uint8_t* const data2 = frame->data[2];
                    const int linesize1 = frame->linesize[1];
                    const int linesize2 = frame->linesize[2];
                    for (std::size_t i = 0; i < h; ++i)
                    {
                        std::memcpy(
                            data + w * i,
                            data0 + linesize0 * i,
                            w);
                    }
                    for (std::size_t i = 0; i < h2; ++i)
                    {
                        std::memcpy(
                            data + (w * h) + w2 * i,
                            data1 + linesize1 * i,
                            w2);
                        std::memcpy(
                            data + (w * h) + (w2 * h2) + w2 * i,
                            data2 + linesize2 * i,
                            w2);
                    }
                    break;
                }
                default: break;
                }
            }
            else
            {
                if (!_swsContext)
                {
                    // Build the scaler now that the real source format is known
                    // (the hardware download format, or a software-fallback format).
                    _initSws(static_cast<AVPixelFormat>(frame->format));
                }
                av_image_fill_arrays(
                    _avFrame2->data,
                    _avFrame2->linesize,
                    data,
                    _avOutputPixelFormat,
                    w,
                    h,
                    1);
                frame->color_range =
                    (AVCOL_RANGE_JPEG == _avCodecContext[_avStream]->color_range)
                    ? AVCOL_RANGE_JPEG
                    : AVCOL_RANGE_MPEG;
                frame->colorspace =
                    (AVCOL_SPC_BT2020_NCL == _avCodecParameters[_avStream]->color_space)
                    ? AVCOL_SPC_BT2020_NCL
                    : AVCOL_SPC_BT709;
                sws_scale_frame(_swsContext, _avFrame2, frame);
            }
        }
    }
}

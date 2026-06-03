// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/FFmpegPrivate.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>

extern "C"
{
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/hdr_dynamic_metadata.h>
#include <libavutil/imgutils.h>
#include <libavutil/mastering_display_metadata.h>
}

namespace tl
{
    namespace ffmpeg
    {
        bool Options::operator == (const Options& other) const
        {
            return
                yuvToRgb == other.yuvToRgb &&
                threadCount == other.threadCount;
        }

        bool Options::operator != (const Options& other) const
        {
            return !(*this == other);
        }

        IOOptions getOptions(const Options& value)
        {
            IOOptions out;
            out["FFmpeg/YUVToRGB"] = ftk::Format("{0}").arg(value.yuvToRgb);
            out["FFmpeg/ThreadCount"] = ftk::Format("{0}").arg(value.threadCount);
            return out;
        }

        AVRational swap(AVRational value)
        {
            return AVRational({ value.den, value.num });
        }

        void toHDRData(AVFrameSideData** sideData, int size, HDRData& hdr)
        {
            for (int i = 0; i < size; ++i)
            {
                switch (sideData[i]->type)
                {
                case AV_FRAME_DATA_MASTERING_DISPLAY_METADATA:
                {
                    auto data = reinterpret_cast<AVMasteringDisplayMetadata*>(sideData[i]->data);
                    hdr.displayMasteringLuminance = ftk::RangeF(
                        data->min_luminance.num / data->min_luminance.den,
                        data->max_luminance.num / data->max_luminance.den);
                    break;
                }
                case AV_FRAME_DATA_CONTENT_LIGHT_LEVEL:
                {
                    auto data = reinterpret_cast<AVContentLightMetadata*>(sideData[i]->data);
                    hdr.maxCLL = data->MaxCLL;
                    hdr.maxFALL = data->MaxFALL;
                    break;
                }
                case AV_FRAME_DATA_DYNAMIC_HDR_PLUS:
                {
                    auto data = reinterpret_cast<AVDynamicHDRPlus*>(sideData[i]->data);
                    break;
                }
                default: break;
                }
            }
        }

        AudioType toAudioType(AVSampleFormat value)
        {
            AudioType out = AudioType::None;
            switch (value)
            {
            case AV_SAMPLE_FMT_S16:  out = AudioType::S16; break;
            case AV_SAMPLE_FMT_S32:  out = AudioType::S32; break;
            case AV_SAMPLE_FMT_FLT:  out = AudioType::F32; break;
            case AV_SAMPLE_FMT_DBL:  out = AudioType::F64; break;
            case AV_SAMPLE_FMT_S16P: out = AudioType::S16; break;
            case AV_SAMPLE_FMT_S32P: out = AudioType::S32; break;
            case AV_SAMPLE_FMT_FLTP: out = AudioType::F32; break;
            case AV_SAMPLE_FMT_DBLP: out = AudioType::F64; break;
            default: break;
            }
            return out;
        }

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

        std::string getTimecodeFromDataStream(AVFormatContext* avFormatContext)
        {
            int dataStream = -1;
            for (unsigned int i = 0; i < avFormatContext->nb_streams; ++i)
            {
                if (AVMEDIA_TYPE_DATA == avFormatContext->streams[i]->codecpar->codec_type &&
                    AV_DISPOSITION_DEFAULT == avFormatContext->streams[i]->disposition)
                {
                    dataStream = i;
                    break;
                }
            }
            if (-1 == dataStream)
            {
                for (unsigned int i = 0; i < avFormatContext->nb_streams; ++i)
                {
                    if (AVMEDIA_TYPE_DATA == avFormatContext->streams[i]->codecpar->codec_type)
                    {
                        dataStream = i;
                        break;
                    }
                }
            }
            std::string timecode;
            if (dataStream != -1)
            {
                AVDictionaryEntry* tag = nullptr;
                while ((tag = av_dict_get(
                    avFormatContext->streams[dataStream]->metadata,
                    "",
                    tag,
                    AV_DICT_IGNORE_SUFFIX)))
                {
                    if (ftk::compare(
                        tag->key,
                        "timecode",
                        ftk::CaseCompare::Insensitive))
                    {
                        timecode = tag->value;
                        break;
                    }
                }
            }
            return timecode;
        }

        Packet::Packet()
        {
            p = av_packet_alloc();
        }

        Packet::~Packet()
        {
            av_packet_free(&p);
        }

        std::string getErrorLabel(int r)
        {
            char buf[ftk::cStringSize];
            av_strerror(r, buf, ftk::cStringSize);
            return std::string(buf);
        }

        std::weak_ptr<ftk::LogSystem> ReadPlugin::_logSystemWeak;

        struct ReadPlugin::Private
        {
            std::vector<AVCodecID> codecIds;
            std::vector<std::string> codecNames;
        };

        void ReadPlugin::_init(const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            FTK_P();

            // Get codecs.
            const AVCodec* avCodec = nullptr;
            void* avCodecIterate = nullptr;
            while ((avCodec = av_codec_iterate(&avCodecIterate)))
            {
                if ((AVMEDIA_TYPE_VIDEO == avCodec->type || AVMEDIA_TYPE_AUDIO == avCodec->type) &&
                    av_codec_is_decoder(avCodec))
                {
                    p.codecIds.push_back(avCodec->id);
                    p.codecNames.push_back(avCodec->name);
                }
            }

            // Get formats.
            std::map<std::string, FileType> extensions;
            const AVInputFormat* avInputFormat = nullptr;
            void* avInputFormatIterate = nullptr;
            std::vector<std::string> formatLog;
            while ((avInputFormat = av_demuxer_iterate(&avInputFormatIterate)))
            {
                if (avInputFormat->extensions)
                {
                    for (auto extension : ftk::split(avInputFormat->extensions, ','))
                    {
                        if (!extension.empty() && extension[0] != '.')
                        {
                            extension.insert(0, ".");
                        }
                        extensions[extension] = FileType::Media;
                    }
                    formatLog.push_back(ftk::Format("{0} ({1})").arg(avInputFormat->name).arg(avInputFormat->extensions));
                }
            }
            //! \bug Why aren't these in the list of input formats?
            extensions[".m4v"] = FileType::Media;
            extensions[".mxf"] = FileType::Media;
            extensions[".wav"] = FileType::Media;

            IReadPlugin::_init("FFmpeg", extensions, logSystem);

            _logSystemWeak = logSystem;
            //av_log_set_level(AV_LOG_QUIET);
            av_log_set_level(AV_LOG_VERBOSE);
            av_log_set_callback(_logCallback);

            logSystem->print(
                "tl::ffmpeg::ReadPlugin",
                ftk::Format(
                    "\n"
                    "    * Codecs: {0}\n"
                    "    * Formats: {1}").
                arg(ftk::join(p.codecNames, ", ")).
                arg(ftk::join(formatLog, ", ")));
        }

        ReadPlugin::ReadPlugin() :
            _p(new Private)
        {}

        std::shared_ptr<ReadPlugin> ReadPlugin::create(
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            auto out = std::shared_ptr<ReadPlugin>(new ReadPlugin);
            out->_init(logSystem);
            return out;
        }

        std::shared_ptr<IRead> ReadPlugin::read(
            const ftk::Path& path,
            const IOOptions& options)
        {
            return Read::create(path, options, _logSystem.lock());
        }

        std::shared_ptr<IRead> ReadPlugin::read(
            const ftk::Path& path,
            const std::vector<ftk::MemFile>& memory,
            const IOOptions& options)
        {
            return Read::create(path, memory, options, _logSystem.lock());
        }

        void ReadPlugin::_logCallback(void*, int level, const char* fmt, va_list vl)
        {
            switch (level)
            {
            case AV_LOG_PANIC:
            case AV_LOG_FATAL:
            case AV_LOG_ERROR:
            case AV_LOG_WARNING:
            case AV_LOG_INFO:
                if (auto logSystem = _logSystemWeak.lock())
                {
                    char buf[ftk::cStringSize];
                    vsnprintf(buf, ftk::cStringSize, fmt, vl);
                    std::string s(buf);
                    ftk::removeTrailingNewlines(s);
                    logSystem->print("tl::ffmpeg::ReadPlugin", s);
                }
                break;
            case AV_LOG_VERBOSE:
            default: break;
            }
        }

        std::weak_ptr<ftk::LogSystem> WritePlugin::_logSystemWeak;

        struct WritePlugin::Private
        {
            std::vector<AVCodecID> codecIds;
            std::vector<std::string> codecNames;
        };

        void WritePlugin::_init(
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            FTK_P();

            // Get codecs.
            const AVCodec* avCodec = nullptr;
            void* avCodecIterate = nullptr;
            while ((avCodec = av_codec_iterate(&avCodecIterate)))
            {
                if (AVMEDIA_TYPE_VIDEO == avCodec->type && av_codec_is_encoder(avCodec))
                {
                    p.codecIds.push_back(avCodec->id);
                    p.codecNames.push_back(avCodec->name);
                }
            }

            // Get formats.
            std::map<std::string, FileType> extensions;
            const AVOutputFormat* avOutputFormat = nullptr;
            void* avOutputFormatIterate = nullptr;
            std::vector<std::string> formatLog;
            while ((avOutputFormat = av_muxer_iterate(&avOutputFormatIterate)))
            {
                if (avOutputFormat->extensions)
                {
                    for (auto extension : ftk::split(avOutputFormat->extensions, ','))
                    {
                        if (!extension.empty() && extension[0] != '.')
                        {
                            extension.insert(0, ".");
                        }                            
                        extensions[extension] = FileType::Media;
                    }
                    formatLog.push_back(ftk::Format("{0} ({1})").arg(avOutputFormat->name).arg(avOutputFormat->extensions));
                }
            }

            IWritePlugin::_init("FFmpeg", extensions, logSystem);

            //_logSystemWeak = logSystem;
            //av_log_set_level(AV_LOG_QUIET);
            //av_log_set_level(AV_LOG_VERBOSE);
            //av_log_set_callback(_logCallback);

            logSystem->print(
                "tl::ffmpeg::WritePlugin",
                ftk::Format(
                    "\n"
                    "    * Codecs: {0}\n"
                    "    * Formats: {1}").
                arg(ftk::join(p.codecNames, ", ")).
                arg(ftk::join(formatLog, ", ")));
        }

        WritePlugin::WritePlugin() :
            _p(new Private)
        {}

        std::shared_ptr<WritePlugin> WritePlugin::create(
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            auto out = std::shared_ptr<WritePlugin>(new WritePlugin);
            out->_init(logSystem);
            return out;
        }

        const std::vector<std::string>& WritePlugin::getCodecs() const
        {
            return _p->codecNames;
        }

        ftk::ImageInfo WritePlugin::getInfo(
            const ftk::ImageInfo& info,
            const IOOptions& options) const
        {
            ftk::ImageInfo out;
            out.size = info.size;
            switch (info.type)
            {
            case ftk::ImageType::L_U8:
            case ftk::ImageType::L_U16:
            case ftk::ImageType::RGB_U8:
            case ftk::ImageType::RGB_U16:
            case ftk::ImageType::RGBA_U8:
            case ftk::ImageType::RGBA_U16:
                out.type = info.type;
                break;
            default:
                out.type = ftk::ImageType::RGBA_U8;
                break;
            }
            return out;
        }

        std::shared_ptr<IWrite> WritePlugin::write(
            const ftk::Path& path,
            const IOInfo& info,
            const IOOptions& options)
        {
            if (info.video.empty() || (!info.video.empty() && !_isCompatible(info.video[0], options)))
                throw std::runtime_error(ftk::Format("Unsupported video: \"{0}\"").
                    arg(path.get()));
            return Write::create(path, info, options, _logSystem.lock());
        }

        void WritePlugin::_logCallback(void*, int level, const char* fmt, va_list vl)
        {
            switch (level)
            {
            case AV_LOG_PANIC:
            case AV_LOG_FATAL:
            case AV_LOG_ERROR:
            case AV_LOG_WARNING:
            case AV_LOG_INFO:
                if (auto logSystem = _logSystemWeak.lock())
                {
                    char buf[ftk::cStringSize];
                    vsnprintf(buf, ftk::cStringSize, fmt, vl);
                    std::string s(buf);
                    ftk::removeTrailingNewlines(s);
                    logSystem->print("tl::io::ffmpeg::WritePlugin", s);
                }
                break;
            case AV_LOG_VERBOSE:
            default: break;
            }
        }

        void to_json(nlohmann::json& json, const Options& value)
        {
            json["YUVToRGB"] = value.yuvToRgb;
            json["ThreadCount"] = value.threadCount;
        }

        void from_json(const nlohmann::json& json, Options& value)
        {
            json.at("YUVToRGB").get_to(value.yuvToRgb);
            json.at("ThreadCount").get_to(value.threadCount);
        }
    }
}

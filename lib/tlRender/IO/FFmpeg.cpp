// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/FFmpegPrivate.h>
#include <tlRender/IO/FFmpegReadPrivate.h>

#include <tlRender/IO/FFmpegCmd.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/ffversion.h>
#include <libavutil/hdr_dynamic_metadata.h>
#include <libavutil/imgutils.h>
#include <libavutil/mastering_display_metadata.h>
}

namespace tl
{
    namespace ffmpeg
    {

        bool hasHWDecode()
        {
            const AVCodec* avCodec = nullptr;
            void* avCodecIterate = nullptr;
            while ((avCodec = av_codec_iterate(&avCodecIterate)))
            {
                if (av_codec_is_decoder(avCodec) &&
                    avcodec_get_hw_config(avCodec, 0))
                {
                    return true;
                }
            }
            return false;
        }

        IOOptions getOptions(const Options& value)
        {
            IOOptions out;
            out["FFmpeg/YUVToRGB"] = ftk::Format("{0}").arg(value.yuvToRgb);
            out["FFmpeg/HWAccel"] = ftk::Format("{0}").arg(value.hwAccel);
            out["FFmpeg/ThreadCount"] = ftk::Format("{0}").arg(value.threadCount);
            out["FFmpeg/AudioMerge"] = ftk::Format("{0}").arg(value.audioMerge);
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

        std::string getTimecode(AVFormatContext* avFormatContext)
        {
            // Where a movie keeps its start timecode depends on what wrote
            // it: on the video stream, on a timecode track (a data stream,
            // QuickTime's "tmcd"), or on the container. Every data stream is
            // looked at rather than only the first: a subtitle or chapter
            // track can come before the timecode track, and reading only
            // that one found nothing and started the movie at zero (DJV
            // #856).
            const auto find = [](AVDictionary* metadata)
            {
                std::string out;
                AVDictionaryEntry* tag = nullptr;
                while ((tag = av_dict_get(metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
                {
                    if (ftk::compare(
                        tag->key,
                        "timecode",
                        ftk::CaseCompare::Insensitive))
                    {
                        out = tag->value;
                        break;
                    }
                }
                return out;
            };
            std::string out;
            for (unsigned int i = 0; out.empty() && i < avFormatContext->nb_streams; ++i)
            {
                if (AVMEDIA_TYPE_VIDEO == avFormatContext->streams[i]->codecpar->codec_type)
                {
                    out = find(avFormatContext->streams[i]->metadata);
                }
            }
            for (unsigned int i = 0; out.empty() && i < avFormatContext->nb_streams; ++i)
            {
                if (AVMEDIA_TYPE_DATA == avFormatContext->streams[i]->codecpar->codec_type)
                {
                    out = find(avFormatContext->streams[i]->metadata);
                }
            }
            if (out.empty())
            {
                out = find(avFormatContext->metadata);
            }
            return out;
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
            // These extensions need to be added manually:
            extensions[".m4v"] = FileType::Media;
            extensions[".mxf"] = FileType::Media;
            extensions[".wav"] = FileType::Audio;

            // The demuxers do not say what they carry, so the audio
            // extensions are named; the same list as the command line
            // plugin. Only what a demuxer claimed is reclassified, so the
            // extensions stay those of the FFmpeg that was built.
            for (const auto& audioExt :
                { ".aac", ".aiff", ".flac", ".mp3", ".m4a", ".ogg" })
            {
                const auto i = extensions.find(audioExt);
                if (i != extensions.end())
                {
                    i->second = FileType::Audio;
                }
            }

            // What the command line can open as well. The library's list is
            // whatever demuxers this build has, and a minimal build has
            // fewer of them, so claiming only those would stop offering
            // files the user's own ffmpeg could read -- which is what the
            // command line path is for.
            for (const auto& ext : {
                ".avi", ".avif", ".gif", ".mkv", ".mov", ".mp4", ".mxf",
                ".m4v", ".webm", ".y4m" })
            {
                if (extensions.find(ext) == extensions.end())
                {
                    extensions[ext] = FileType::Media;
                }
            }
            for (const auto& ext : {
                ".aac", ".aiff", ".flac", ".mp3", ".m4a", ".ogg", ".wav" })
            {
                if (extensions.find(ext) == extensions.end())
                {
                    extensions[ext] = FileType::Audio;
                }
            }

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

        namespace
        {
            //! Whether the command line reads this file rather than the
            //! library.
            //!
            //! What decides is whether this build has a decoder for the
            //! stream, not what the user prefers: the bundled FFmpeg is built
            //! small on purpose, and the command line is there for what it
            //! cannot decode. Asking the file means nobody has to know which
            //! of their files need which.
            bool useCommandLine(
                const ftk::Path& path,
                const std::vector<ftk::MemFile>& memory,
                const IOOptions& options,
                AVMediaType type)
            {
                if (auto i = options.find("FFmpeg/CommandLine");
                    i != options.end())
                {
                    if ("Always" == i->second)
                        return true;
                    if ("Never" == i->second)
                        return false;
                }

                bool out = false;
                const std::string fileName =
                    path.hasProtocol() ? path.get() : path.getFileName(true);
                AVFormatContext* avFormatContext = nullptr;
                AVIOBufferData avIOBufferData;
                AVIOContext* avIOContext = nullptr;
                if (!memory.empty())
                {
                    // Bundled media is probed from memory, the way the
                    // library reads it, not through the subfile URL the
                    // command line gets. FFmpeg's subfile protocol clips a
                    // seek to thirty-two bits (subfile_seek uses av_clip, in
                    // 8.1 and 9.0 alike), so a bundled movie whose moov atom
                    // sits past 2 GB does not open through it -- and probed
                    // that way, a file the library reads fine was handed to
                    // the command line, which cannot read it either.
                    avFormatContext = avformat_alloc_context();
                    if (!avFormatContext)
                    {
                        return false;
                    }
                    avIOBufferData = AVIOBufferData(memory[0].p, memory[0].size);
                    avIOContext = avio_alloc_context(
                        static_cast<uint8_t*>(av_malloc(avIOContextBufferSize)),
                        avIOContextBufferSize,
                        0,
                        &avIOBufferData,
                        &avIOBufferRead,
                        nullptr,
                        &avIOBufferSeek);
                    if (!avIOContext)
                    {
                        avformat_free_context(avFormatContext);
                        return false;
                    }
                    avFormatContext->pb = avIOContext;
                }
                if (avformat_open_input(
                    &avFormatContext,
                    memory.empty() ? fileName.c_str() : nullptr,
                    nullptr,
                    nullptr) == 0)
                {
                    if (avformat_find_stream_info(avFormatContext, nullptr) >= 0)
                    {
                        const int stream = av_find_best_stream(
                            avFormatContext, type, -1, -1, nullptr, 0);
                        if (stream >= 0)
                        {
                            // No decoder for what the file holds: this build
                            // can see the stream and cannot read it.
                            out = !avcodec_find_decoder(
                                avFormatContext->streams[stream]->codecpar->codec_id);
                        }
                    }
                    avformat_close_input(&avFormatContext);
                }
                else
                {
                    // Not something the library can even open, so the command
                    // line is the only one left to try.
                    out = true;
                }
                if (avIOContext)
                {
                    av_freep(&avIOContext->buffer);
                    avio_context_free(&avIOContext);
                }
                return out;
            }
        }

        std::shared_ptr<IVideoRead> ReadPlugin::videoRead(
            const ftk::Path& path,
            const IOOptions& options)
        {
            auto logSystem = _logSystem.lock();
            if (useCommandLine(path, {}, options, AVMEDIA_TYPE_VIDEO))
            {
                // Said out loud: which of the two read a file is the first
                // thing wanted when it is read wrongly, and it is otherwise
                // not visible from the outside.
                if (logSystem)
                {
                    logSystem->print(
                        "tl::ffmpeg::ReadPlugin",
                        ftk::Format("Reading video with the command line: \"{0}\"").
                        arg(path.get()));
                }
                return ffmpeg_cmd::VideoRead::create(path, options, logSystem);
            }
            return VideoRead::create(path, options, logSystem);
        }

        namespace
        {
            //! A subfile URL for memory that is a window into a file on
            //! disk, or empty. FFmpeg's subfile protocol reads the byte
            //! range in place, which is how the command line reads media
            //! out of an OTIOZ bundle: a sub-process cannot be handed
            //! RAM, but it can be handed the bundle and an offset --
            //! the bytes are in the bundle uncompressed, so the range is
            //! the file. Only single-file media: a bundled image
            //! sequence decodes with the image readers, which are always
            //! built in.
            std::string getSubfileName(const std::vector<ftk::MemFile>& memory)
            {
                std::string out;
                if (1 == memory.size() && !memory[0].path.empty())
                {
                    out = ftk::Format("subfile,,start,{0},end,{1},,:{2}").
                        arg(memory[0].offset).
                        arg(memory[0].offset + memory[0].size).
                        arg(memory[0].path);
                }
                return out;
            }
        }

        std::shared_ptr<IVideoRead> ReadPlugin::videoRead(
            const ftk::Path& path,
            const std::vector<ftk::MemFile>& memory,
            const IOOptions& options)
        {
            if (memory.empty())
            {
                return videoRead(path, options);
            }
            auto logSystem = _logSystem.lock();
            const std::string subfile = getSubfileName(memory);
            if (!subfile.empty() &&
                useCommandLine(path, memory, options, AVMEDIA_TYPE_VIDEO))
            {
                if (logSystem)
                {
                    logSystem->print(
                        "tl::ffmpeg::ReadPlugin",
                        ftk::Format("Reading video with the command line: \"{0}\"").
                        arg(subfile));
                }
                return ffmpeg_cmd::VideoRead::create(
                    ftk::Path(subfile), options, logSystem);
            }
            return VideoRead::create(path, memory, options, logSystem);
        }

        std::shared_ptr<IAudioRead> ReadPlugin::audioRead(
            const ftk::Path& path,
            const IOOptions& options)
        {
            auto logSystem = _logSystem.lock();
            if (useCommandLine(path, {}, options, AVMEDIA_TYPE_AUDIO))
            {
                if (logSystem)
                {
                    logSystem->print(
                        "tl::ffmpeg::ReadPlugin",
                        ftk::Format("Reading audio with the command line: \"{0}\"").
                        arg(path.get()));
                }
                return ffmpeg_cmd::AudioRead::create(path, options, logSystem);
            }
            return AudioRead::create(path, options, logSystem);
        }

        std::shared_ptr<IAudioRead> ReadPlugin::audioRead(
            const ftk::Path& path,
            const std::vector<ftk::MemFile>& memory,
            const IOOptions& options)
        {
            if (memory.empty())
            {
                return audioRead(path, options);
            }
            auto logSystem = _logSystem.lock();
            const std::string subfile = getSubfileName(memory);
            if (!subfile.empty() &&
                useCommandLine(path, memory, options, AVMEDIA_TYPE_AUDIO))
            {
                if (logSystem)
                {
                    logSystem->print(
                        "tl::ffmpeg::ReadPlugin",
                        ftk::Format("Reading audio with the command line: \"{0}\"").
                        arg(subfile));
                }
                return ffmpeg_cmd::AudioRead::create(
                    ftk::Path(subfile), options, logSystem);
            }
            return AudioRead::create(path, memory, options, logSystem);
        }

        std::string ReadPlugin::getPluginInfo(const IOOptions& ioOptions) const
        {
            // Both, when there is a command line to ask: the two read
            // different files and it is worth seeing which versions are in
            // play.
            std::string out = FFMPEG_VERSION;
            const std::string cmd =
                ffmpeg_cmd::getVersion(ioOptions, _logSystem.lock());
            if (!cmd.empty())
            {
                out = ftk::Format("{0} (command line {1})").arg(out).arg(cmd);
            }
            return out;
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

        struct WritePlugin::Private
        {
            std::vector<AVCodecID> codecIds;
            std::vector<std::string> codecNames;
            std::vector<AVCodecID> audioCodecIds;
            std::vector<std::string> audioCodecNames;
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
                else if (AVMEDIA_TYPE_AUDIO == avCodec->type && av_codec_is_encoder(avCodec))
                {
                    p.audioCodecIds.push_back(avCodec->id);
                    p.audioCodecNames.push_back(avCodec->name);
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

            logSystem->print(
                "tl::ffmpeg::WritePlugin",
                ftk::Format(
                    "\n"
                    "    * Codecs: {0}\n"
                    "    * Audio codecs: {1}\n"
                    "    * Formats: {2}").
                arg(ftk::join(p.codecNames, ", ")).
                arg(ftk::join(p.audioCodecNames, ", ")).
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

        const std::vector<std::string>& WritePlugin::getAudioCodecs() const
        {
            return _p->audioCodecNames;
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
            // Writing with the command line is an explicit choice, unlike
            // reading, which asks the file: the caller says which encoder
            // family does the work.
            if (auto i = options.find("FFmpeg/WriteCommandLine");
                i != options.end() && "1" == i->second)
            {
                auto logSystem = _logSystem.lock();
                if (logSystem)
                {
                    logSystem->print(
                        "tl::ffmpeg::WritePlugin",
                        ftk::Format("Writing video with the command line: \"{0}\"").
                        arg(path.get()));
                }
                return ffmpeg_cmd::Write::create(path, info, options, logSystem);
            }
            if (info.video.empty() || (!info.video.empty() && !_isCompatible(info.video[0], options)))
                throw std::runtime_error(ftk::Format("Unsupported video: \"{0}\"").
                    arg(path.get()));
            return Write::create(path, info, options, _logSystem.lock());
        }

        std::string WritePlugin::getPluginInfo(const IOOptions&) const
        {
            return FFMPEG_VERSION;
        }

        void to_json(nlohmann::json& json, const Options& value)
        {
            json["YUVToRGB"] = value.yuvToRgb;
            json["HWAccel"] = value.hwAccel;
            json["ThreadCount"] = value.threadCount;
            json["AudioMerge"] = value.audioMerge;
        }

        void from_json(const nlohmann::json& json, Options& value)
        {
            json.at("YUVToRGB").get_to(value.yuvToRgb);
            if (json.contains("HWAccel"))
            {
                json.at("HWAccel").get_to(value.hwAccel);
            }
            json.at("ThreadCount").get_to(value.threadCount);
            if (json.contains("AudioMerge"))
            {
                json.at("AudioMerge").get_to(value.audioMerge);
            }
        }
    }
}

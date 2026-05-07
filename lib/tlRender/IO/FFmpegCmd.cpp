// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/FFmpegCmdPrivate.h>

#include <ftk/Core/Format.h>
#include <ftk/Core/String.h>

#include <subprocess.h>

#include <regex>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX
#include <Windows.h>
#endif

namespace tl
{
    namespace ffmpeg_cmd
    {
#if defined(_WIN32)
        namespace
        {
            std::string toSubprocessArg(const std::string& value)
            {
                if (value.empty())
                {
                    return value;
                }

                // subprocess.h uses CreateProcessA on Windows. Convert tlRender's
                // UTF-8 strings to the active ANSI code page before handing off so
                // CreateProcessA reconstructs the intended wide command line.
                const std::wstring wide = ftk::toWide(value);
                const int size = WideCharToMultiByte(
                    CP_ACP,
                    0,
                    wide.c_str(),
                    -1,
                    nullptr,
                    0,
                    nullptr,
                    nullptr);
                if (size <= 0)
                {
                    return value;
                }

                std::string out(size, 0);
                const int written = WideCharToMultiByte(
                    CP_ACP,
                    0,
                    wide.c_str(),
                    -1,
                    out.data(),
                    size,
                    nullptr,
                    nullptr);
                if (written <= 0)
                {
                    return value;
                }
                if (!out.empty() && '\0' == out.back())
                {
                    out.pop_back();
                }
                return out;
            }
        }
#endif

        Options::Options(const IOOptions& value)
        {
            auto i = value.find("FFmpeg/FFmpegPath");
            if (i != value.end())
            {
                std::stringstream ss(i->second);
                ss >> ffmpegPath;
            }
            i = value.find("FFmpeg/FFprobePath");
            if (i != value.end())
            {
                std::stringstream ss(i->second);
                ss >> ffprobePath;
            }
        }

        IOOptions Options::getIOOptions() const
        {
            IOOptions out;
            out["FFmpeg/FFmpegPath"] = ffmpegPath;
            out["FFmpeg/FFprobePath"] = ffprobePath;
            return out;
        }

        bool Options::operator == (const Options& other) const
        {
            return
                ffmpegPath == other.ffmpegPath &&
                ffprobePath == other.ffprobePath;
        }

        bool Options::operator != (const Options& other) const
        {
            return !(*this == other);
        }

        Rational toRational(const std::string& value)
        {
            Rational out;
            const auto s = ftk::split(value, '/');
            if (2 == s.size())
            {
                out.first = atoi(s[0].c_str());
                out.second = atoi(s[1].c_str());
            }
            return out;
        }

        double toDouble(const Rational& value)
        {
            return value.second != 0 ?
                (value.first / static_cast<double>(value.second)) :
                0.0;
        }

        ftk::ImageType toImageType(const std::string& value)
        {
            ftk::ImageType out = ftk::ImageType::None;
            if ("gray8" == value)
            {
                out = ftk::ImageType::L_U8;
            }
            else if ("rgb24" == value)
            {
                out = ftk::ImageType::RGB_U8;
            }
            else if ("rgba" == value)
            {
                out = ftk::ImageType::RGBA_U8;
            }
            return out;
        }

        std::string fromImageType(ftk::ImageType value)
        {
            std::string out;
            switch (value)
            {
            case ftk::ImageType::L_U8: out = "gray8"; break;
            case ftk::ImageType::RGB_U8: out = "rgb24"; break;
            case ftk::ImageType::RGBA_U8: out = "rgba"; break;
            default: break;
            }
            return out;
        }

        AudioType toAudioType(const std::string& value)
        {
            AudioType out = AudioType::None;
            if ("s16" == value || "s16p" == value)
            {
                out = AudioType::S16;
            }
            else if ("s32" == value || "s32p" == value)
            {
                out = AudioType::S32;
            }
            else if ("flt" == value || "fltp" == value)
            {
                out = AudioType::F32;
            }
            else if ("dbl" == value || "dblp" == value)
            {
                out = AudioType::F64;
            }
            return out;
        }

        std::string fromAudioType(AudioType value)
        {
            std::string out;
            switch (value)
            {
                case AudioType::S16: out = "s16"; break;
                case AudioType::S32: out = "s32"; break;
                case AudioType::F32: out = "f32"; break;
                case AudioType::F64: out = "f64"; break;
                default: break;
            }
            if (!out.empty())
            {
                switch (ftk::getEndian())
                {
                    case ftk::Endian::LSB: out += "le"; break;
                    case ftk::Endian::MSB: out += "be"; break;
                    default: break;
                }
            }
            return out;
        }

        struct Pipe::Private
        {
            subprocess_s subprocess;
        };

        Pipe::Pipe(const std::vector<std::string>& cmd) :
            _p(new Private)
        {
            FTK_P();
#if defined(_WIN32)
            std::vector<std::string> command = cmd;
            for (auto& i : command)
            {
                i = toSubprocessArg(i);
            }
#else
            const auto& command = cmd;
#endif
            std::vector<const char*> args;
            for (const auto& i : command)
            {
                args.push_back(i.c_str());
            }
            args.push_back(nullptr);
            int r = subprocess_create(
                args.data(),
                subprocess_option_inherit_environment |
                subprocess_option_search_user_path |
                subprocess_option_enable_async |
                subprocess_option_no_window,
                &p.subprocess);
            if (r != 0)
            {
                throw std::runtime_error(ftk::Format("Cannot run command: \"{0}\"").
                    arg(ftk::join(cmd, ' ')));
            }
        }

        Pipe::~Pipe()
        {
            FTK_P();
            subprocess_destroy(&p.subprocess);
            //! \bug This is causing a SIGKILL on Linux when a read error occurs.
            //subprocess_terminate(&p.subprocess);
        }

        size_t Pipe::read(uint8_t* data, size_t size)
        {
            FTK_P();
            if (0 == subprocess_alive(&p.subprocess))
            {
                throw std::runtime_error("Cannot read from process");
            }
            size_t out = 0;
            size_t r = 0;
            do
            {
                r = subprocess_read_stdout(
                    &p.subprocess,
                    reinterpret_cast<char*>(data + out),
                    size - out);
                out += r;
            } while (out < size && r > 0);
            return out;
        }

        std::string Pipe::readAll()
        {
            FTK_P();
            if (0 == subprocess_alive(&p.subprocess))
            {
                throw std::runtime_error("Cannot read from process");
            }
            std::string out;
            size_t size = 0;
            do
            {
                const size_t chunkSize = 1024;
                std::string chunk(chunkSize, 0);
                size = subprocess_read_stdout(&p.subprocess, chunk.data(), chunkSize);
                chunk.resize(size);
                out.insert(out.end(), chunk.begin(), chunk.end());
            }
            while (size > 0);
            return out;
        }

        void ReadPlugin::_init(const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            std::map<std::string, FileType> extensions;
            extensions[".avi"] = FileType::Media;
            extensions[".avif"] = FileType::Media;
            extensions[".gif"] = FileType::Media;
            extensions[".mkv"] = FileType::Media;
            extensions[".mov"] = FileType::Media;
            extensions[".mp4"] = FileType::Media;
            extensions[".mxf"] = FileType::Media;
            extensions[".m4v"] = FileType::Media;
            extensions[".webm"] = FileType::Media;
            extensions[".y4m"] = FileType::Media;

            extensions[".aac"] = FileType::Media;
            extensions[".aiff"] = FileType::Media;
            extensions[".flac"] = FileType::Media;
            extensions[".mp3"] = FileType::Media;
            extensions[".m4a"] = FileType::Media;
            extensions[".ogg"] = FileType::Media;
            extensions[".wav"] = FileType::Media;
            IReadPlugin::_init("FFmpegCmd", extensions, logSystem);
        }

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

        void to_json(nlohmann::json& json, const Options& value)
        {
            json["FFprobe"] = value.ffprobePath;
            json["FFmpeg"] = value.ffmpegPath;
        }

        void from_json(const nlohmann::json& json, Options& value)
        {
            json.at("FFprobe").get_to(value.ffprobePath);
            json.at("FFmpeg").get_to(value.ffmpegPath);
        }
    }
}

// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/FFmpegCmdPrivate.h>

#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>
#include <ftk/Core/String.h>

#include <subprocess.h>

#include <mutex>

#include <regex>

namespace tl
{
    namespace ffmpeg_cmd
    {
        Options::Options(const IOOptions& value)
        {
            if (auto i = value.find("FFmpeg/FFmpegPath"); i != value.end())
            {
                std::stringstream ss(i->second);
                ss >> ffmpegPath;
            }
            if (auto i = value.find("FFmpeg/FFprobePath"); i != value.end())
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

        namespace
        {
            // Starting a process is serialized.
            //
            // subprocess_create() now makes its POSIX pipes close-on-exec, so
            // a process started later no longer inherits the pipes of one
            // still running:
            // https://github.com/sheredom/subprocess.h/issues/114
            // Without that a decoder, which lives as long as the movie is
            // open, ended up holding the pipe of a probe that had already
            // exited; the pipe never reached the end of the file and whoever
            // was reading it waited forever.
            //
            // The lock is still needed. Upstream asks for the flag atomically
            // only where pipe2() exists, and macOS has none, so there it makes
            // the pipes and then sets the flag -- a gap another thread
            // starting a process fits through.
            std::mutex& createMutex()
            {
                static std::mutex out;
                return out;
            }

        }

        Pipe::Pipe(const std::vector<std::string>& cmd) :
            _p(new Private)
        {
            FTK_P();
            std::vector<const char*> args;
            for (const auto& i : cmd)
            {
                args.push_back(i.c_str());
            }
            args.push_back(nullptr);
            int r = 0;
            {
                std::unique_lock<std::mutex> lock(createMutex());
                r = subprocess_create(
                    args.data(),
                    subprocess_option_inherit_environment |
                    subprocess_option_search_user_path |
                    subprocess_option_enable_async |
                    subprocess_option_no_window,
                    &p.subprocess);
            }
            if (r != 0)
            {
                throw std::runtime_error(ftk::Format("Cannot run command: \"{0}\"").
                    arg(ftk::join(cmd, ' ')));
            }
        }

        Pipe::~Pipe()
        {
            FTK_P();
            // Reap the child without risking a hang or a stray signal.
            //
            // subprocess_destroy() never waitpid()s on POSIX, so on its own it
            // leaks a zombie. subprocess_join() reaps, but its waitpid() blocks
            // until the child exits -- and a child does not reliably exit just
            // because we closed its stdout (a stalled input, a slow flush), so
            // joining unconditionally can hang teardown.
            //
            // So: if the child is still running, kill it first, which makes the
            // join immediate. The subprocess_alive() check both reaps a child
            // that already exited and guards the terminate: once a child has
            // been reaped its pid is 0, and subprocess_terminate() would then
            // become kill(0, SIGKILL), signalling our entire process group. We
            // only terminate while the child is genuinely alive (a valid pid).
            // This is a read-only decode, so killing ffmpeg cannot corrupt any
            // output.
            if (0 != subprocess_alive(&p.subprocess))
            {
                subprocess_terminate(&p.subprocess);
            }
            subprocess_join(&p.subprocess, nullptr);
            subprocess_destroy(&p.subprocess);
        }

        size_t Pipe::read(uint8_t* data, size_t size)
        {
            FTK_P();
            // Read until the buffer is full or the process closes its stdout.
            // With subprocess_option_enable_async a return of 0 means the
            // process has finished (true EOF), so a short result here signals
            // end-of-stream or a decode error -- the caller checks for it. Note
            // we do NOT precheck subprocess_alive: the process can exit with its
            // output still buffered in the pipe, and refusing to read it would
            // discard valid data.
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
            // Read until EOF; see the note in read() on why we do not precheck
            // subprocess_alive (a fast process can exit with its output still
            // buffered, and the precheck would throw it away).
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

        std::shared_ptr<IVideoRead> ReadPlugin::videoRead(
            const ftk::Path& path,
            const IOOptions& options)
        {
            return VideoRead::create(path, options, _logSystem.lock());
        }

        std::shared_ptr<IVideoRead> ReadPlugin::videoRead(
            const ftk::Path& path,
            const std::vector<ftk::MemFile>& memory,
            const IOOptions& options)
        {
            _memoryUnsupported(path, memory);
            return VideoRead::create(path, memory, options, _logSystem.lock());
        }

        std::shared_ptr<IAudioRead> ReadPlugin::audioRead(
            const ftk::Path& path,
            const IOOptions& options)
        {
            return AudioRead::create(path, options, _logSystem.lock());
        }

        std::shared_ptr<IAudioRead> ReadPlugin::audioRead(
            const ftk::Path& path,
            const std::vector<ftk::MemFile>& memory,
            const IOOptions& options)
        {
            _memoryUnsupported(path, memory);
            return AudioRead::create(path, memory, options, _logSystem.lock());
        }

        void ReadPlugin::_memoryUnsupported(
            const ftk::Path& path,
            const std::vector<ftk::MemFile>& memory)
        {
            // The command line takes a file name, so media held as a byte
            // range -- what a bundle stores -- has nowhere to go. Throwing
            // hands the read to the next plugin for the extension, which on a
            // build with the FFmpeg library is one that can read it; on a
            // build without, it is the error the reader would otherwise have
            // been silent about.
            if (!memory.empty())
            {
                throw std::runtime_error(
                    ftk::Format("The FFmpeg command line cannot read media "
                        "held in memory: \"{0}\"").arg(path.get()));
            }
        }

        std::string ReadPlugin::getPluginInfo(const IOOptions& ioOptions) const
        {
            std::string out;
            try
            {
                const Options options(ioOptions);
                std::vector<std::string> cmd;
                cmd.push_back(options.ffmpegPath);
                cmd.push_back("-v");
                cmd.push_back("quiet");
                cmd.push_back("-version");
                //std::cout << ftk::join(cmd, ' ') << std::endl;
                const auto text = ftk::split(Pipe(cmd).readAll(), '\n');
                if (!text.empty())
                {
                    const auto words = ftk::split(text[0], ' ');
                    if (words.size() >= 3)
                    {
                        out = words[2];
                    }
                }
            }
            catch (const std::exception& e)
            {
                _logSystem.lock()->print("tl::ffmpeg_cmd::ReadPlugin", e.what(), ftk::LogType::Error);
            }
            return out;
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

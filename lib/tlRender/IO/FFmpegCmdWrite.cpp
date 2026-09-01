// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/FFmpegCmdPrivate.h>

#include <tlRender/IO/FFmpegCmd.h>

#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>
#include <ftk/Core/String.h>

#include <sstream>

namespace tl
{
    namespace ffmpeg_cmd
    {
        namespace
        {
            //! The raw video format written to the pipe. Wider than the
            //! reader's map: the writer accepts every packed type the
            //! library writer does.
            std::string writePixelFormat(ftk::ImageType value)
            {
                std::string out;
                switch (value)
                {
                case ftk::ImageType::L_U8: out = "gray8"; break;
                case ftk::ImageType::L_U16: out = "gray16le"; break;
                case ftk::ImageType::RGB_U8: out = "rgb24"; break;
                case ftk::ImageType::RGB_U16: out = "rgb48le"; break;
                case ftk::ImageType::RGBA_U8: out = "rgba"; break;
                case ftk::ImageType::RGBA_U16: out = "rgba64le"; break;
                default: break;
                }
                return out;
            }
        }

        const std::vector<WritePreset>& getWritePresets()
        {
            // References:
            // https://academysoftwarefoundation.github.io/EncodingGuidelines/
            static const std::vector<WritePreset> presets =
            {
                { "H.264",
                    { "-codec:v", "libx264", "-preset", "slow", "-crf", "18",
                      "-pix_fmt", "yuv420p", "-movflags", "+faststart" } },
                { "H.265",
                    { "-codec:v", "libx265", "-preset", "medium", "-crf", "20",
                      "-pix_fmt", "yuv420p", "-tag:v", "hvc1",
                      "-movflags", "+faststart" } },
                { "VP9",
                    { "-codec:v", "libvpx-vp9", "-crf", "31", "-b:v", "0",
                      "-pix_fmt", "yuv420p" } },
                { "AV1",
                    { "-codec:v", "libsvtav1", "-crf", "35",
                      "-pix_fmt", "yuv420p" } }
            };
            return presets;
        }

        struct Write::Private
        {
            std::shared_ptr<Pipe> pipe;
            std::vector<std::string> cmd;
            size_t rowByteCount = 0;
            size_t stride = 0;
            bool finished = false;
            std::shared_ptr<ftk::LogSystem> logSystem;
        };

        void Write::_init(
            const ftk::Path& path,
            const IOOptions& options,
            const IOInfo& info,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            IWrite::_init(path, options, info, logSystem);
            FTK_P();
            p.logSystem = logSystem;

            if (info.video.empty())
            {
                throw std::runtime_error(
                    ftk::Format("No video: \"{0}\"").arg(path.get()));
            }
            const ftk::ImageInfo& imageInfo = info.video[0];
            const std::string pixelFormat = writePixelFormat(imageInfo.type);
            if (pixelFormat.empty())
            {
                throw std::runtime_error(
                    ftk::Format("Unsupported video: \"{0}\"").arg(path.get()));
            }

            const Options ffOptions(options);
            std::vector<std::string> cmd;
            cmd.push_back(ffOptions.ffmpegPath);
            cmd.push_back("-y");
            cmd.push_back("-f");
            cmd.push_back("rawvideo");
            cmd.push_back("-pix_fmt");
            cmd.push_back(pixelFormat);
            cmd.push_back("-video_size");
            cmd.push_back(ftk::Format("{0}x{1}").
                arg(imageInfo.size.w).
                arg(imageInfo.size.h));
            cmd.push_back("-framerate");
            {
                std::stringstream ss;
                ss << (info.videoTime.has_value() ?
                    info.videoTime->duration().rate() : 24.0);
                cmd.push_back(ss.str());
            }
            cmd.push_back("-i");
            cmd.push_back("pipe:0");

            // The encoding arguments: a preset by name, then whatever extra
            // arguments the caller adds. The tags carry the display colour
            // encoding as FFmpeg's own names, so they pass straight through
            // and this path stamps files the way the library writer does.
            const auto& presets = getWritePresets();
            std::string presetName = presets.front().name;
            if (auto i = options.find("FFmpeg/WritePreset"); i != options.end())
            {
                presetName = i->second;
            }
            for (const auto& preset : presets)
            {
                if (preset.name == presetName)
                {
                    cmd.insert(cmd.end(), preset.args.begin(), preset.args.end());
                    break;
                }
            }
            if (auto i = info.tags.find("Color Primaries"); i != info.tags.end())
            {
                cmd.push_back("-color_primaries");
                cmd.push_back(i->second);
            }
            if (auto i = info.tags.find("Color Transfer"); i != info.tags.end())
            {
                cmd.push_back("-color_trc");
                cmd.push_back(i->second);
            }
            if (auto i = info.tags.find("Color Matrix"); i != info.tags.end())
            {
                cmd.push_back("-colorspace");
                cmd.push_back(i->second);
            }
            if (auto i = options.find("FFmpeg/WriteArgs"); i != options.end())
            {
                for (const auto& arg : ftk::split(i->second, ' '))
                {
                    cmd.push_back(arg);
                }
            }
            cmd.push_back(path.get());

            p.rowByteCount = ftk::ImageInfo(
                ftk::Size2I(imageInfo.size.w, 1), imageInfo.type).getByteCount();
            p.stride = imageInfo.size.h > 0 ?
                imageInfo.getByteCount() / imageInfo.size.h : 0;
            p.cmd = cmd;
            if (p.logSystem)
            {
                p.logSystem->print(
                    "tl::ffmpeg_cmd::Write",
                    ftk::Format("Writing with the command line: {0}").
                    arg(ftk::join(cmd, ' ')));
            }
            p.pipe = std::make_shared<Pipe>(cmd);
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
                if (p.logSystem)
                {
                    p.logSystem->print(
                        "tl::ffmpeg_cmd::Write",
                        e.what(),
                        ftk::LogType::Error);
                }
            }
        }

        std::shared_ptr<Write> Write::create(
            const ftk::Path& path,
            const IOInfo& info,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            auto out = std::shared_ptr<Write>(new Write);
            out->_init(path, options, info, logSystem);
            return out;
        }

        void Write::writeVideo(
            const OTIO_NS::RationalTime&,
            const std::shared_ptr<ftk::Image>& image,
            const IOOptions&)
        {
            FTK_P();
            if (!p.pipe || p.finished)
            {
                throw std::runtime_error("Cannot write video: the process is not running");
            }
            // The frames arrive bottom-up, the way the library writer gets
            // them, so the rows go to the pipe in reverse.
            const auto& imageInfo = image->getInfo();
            const uint8_t* data = image->getData();
            for (int y = imageInfo.size.h - 1; y >= 0; --y)
            {
                if (!p.pipe->write(data + y * p.stride, p.rowByteCount))
                {
                    const std::string errors = p.pipe->readAllErrors();
                    p.finished = true;
                    p.pipe.reset();
                    throw std::runtime_error(
                        ftk::Format("Cannot write video: {0}").
                        arg(errors.empty() ? std::string("the process exited") : errors));
                }
            }
        }

        void Write::finish()
        {
            FTK_P();
            if (!p.pipe || p.finished)
            {
                return;
            }
            // Mark as finished up front so that a failed finish is not
            // retried by the destructor.
            p.finished = true;
            const int code = p.pipe->finish();
            if (code != 0)
            {
                const std::string errors = p.pipe->readAllErrors();
                p.pipe.reset();
                throw std::runtime_error(
                    ftk::Format("The command line exited with {0}: {1}").
                    arg(code).
                    arg(errors));
            }
            p.pipe.reset();
        }
    }
}

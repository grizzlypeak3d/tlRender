// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/OIIO.h>

#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>

#include <OpenImageIO/imagebufalgo.h>

namespace tl
{
    namespace oiio
    {
        void ReadPlugin::_init(const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            std::map<std::string, FileType> exts;
            for (const auto& i : OIIO::get_extension_map())
            {
                // Not FFmpeg's, which the FFmpeg plugin reads, and not the
                // formats that are not files to open: "null" makes a blank
                // image and "term" only writes to a terminal.
                if (i.first != "ffmpeg" &&
                    i.first != "null" &&
                    i.first != "term")
                {
                    for (const auto& ext : i.second)
                    {
                        exts["." + ext] = FileType::Seq;
                        if ("raw" == i.first)
                        {
                            _rawExts.insert("." + ext);
                        }
                    }
                }
            }
            IReadPlugin::_init("OIIO", exts, logSystem);

            std::vector<std::string> log;
            for (const auto& i : exts)
            {
                log.push_back(i.first);
            }
            logSystem->print(
                "tl::OIIO::ReadPlugin",
                ftk::Format(
                    "\n"
                    "    * Formats: {0}").arg(ftk::join(log, ", ")));
        }

        std::shared_ptr<ReadPlugin> ReadPlugin::create(
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            auto out = std::shared_ptr<ReadPlugin>(new ReadPlugin);
            out->_init(logSystem);
            return out;
        }



        std::shared_ptr<IDecode> ReadPlugin::decode(const IOOptions&)
        {
            return Decode::create();
        }

        std::string ReadPlugin::getPluginInfo(const IOOptions&) const
        {
            return OIIO_VERSION_STRING;
        }

        std::map<std::string, std::set<std::string> > ReadPlugin::getExtGroups() const
        {
            std::map<std::string, std::set<std::string> > out;
            if (!_rawExts.empty())
            {
                out["Camera Raw"] = _rawExts;
            }
            return out;
        }

        void WritePlugin::_init(const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            std::map<std::string, FileType> exts;
            for (const auto& i : OIIO::get_extension_map())
            {
                for (const auto& ext : i.second)
                {
                    exts["." + ext] = FileType::Seq;
                }
            }
            IWritePlugin::_init("OIIO", exts, logSystem);

            std::vector<std::string> log;
            for (const auto& i : exts)
            {
                log.push_back(i.first);
            }
            logSystem->print(
                "tl::OIIO::WritePlugin",
                ftk::Format(
                    "\n"
                    "    * Formats: {0}").arg(ftk::join(log, ", ")));
        }

        std::shared_ptr<WritePlugin> WritePlugin::create(
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            auto out = std::shared_ptr<WritePlugin>(new WritePlugin);
            out->_init(logSystem);
            return out;
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
            case ftk::ImageType::L_U32:
            case ftk::ImageType::L_F16:
            case ftk::ImageType::L_F32:
            case ftk::ImageType::LA_U8:
            case ftk::ImageType::LA_U16:
            case ftk::ImageType::LA_U32:
            case ftk::ImageType::LA_F16:
            case ftk::ImageType::LA_F32:
            case ftk::ImageType::RGB_U8:
            case ftk::ImageType::RGB_U16:
            case ftk::ImageType::RGB_U32:
            case ftk::ImageType::RGB_F16:
            case ftk::ImageType::RGB_F32:
            case ftk::ImageType::RGBA_U8:
            case ftk::ImageType::RGBA_U16:
            case ftk::ImageType::RGBA_U32:
            case ftk::ImageType::RGBA_F16:
            case ftk::ImageType::RGBA_F32:
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
            {
                throw std::runtime_error(ftk::Format("Unsupported video: \"{0}\"").
                    arg(path.get()));
            }
            return Write::create(path, info, options, _logSystem.lock());
        }

        std::string WritePlugin::getPluginInfo(const IOOptions&) const
        {
            return OIIO_VERSION_STRING;
        }
    }
}

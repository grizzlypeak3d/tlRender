// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/PNG.h>

#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>
#include <ftk/Core/Version.h>

namespace tl
{
    namespace png
    {
        void ReadPlugin::_init(const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            IReadPlugin::_init(
                "PNG",
                { { ".png", FileType::Seq } },
                logSystem);

            logSystem->print(
                "tl::png::ReadPlugin",
                ftk::Format(
                    "\n"
                    "    * Formats: {0}").arg(".png"));
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
            // The feather-tk version rather than libpng's: the codec comes
            // from there, and tlRender does not link libpng itself.
            return FTK_VERSION_FULL;
        }

        void WritePlugin::_init(const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            IWritePlugin::_init(
                "PNG",
                { { ".png", FileType::Seq } },
                logSystem);

            logSystem->print(
                "tl::png::WritePlugin",
                ftk::Format(
                    "\n"
                    "    * Formats: {0}").arg(".png"));
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
            const IOOptions&) const
        {
            ftk::ImageInfo out;
            out.size = info.size;
            switch (info.type)
            {
            // What PNG stores, and what feather-tk's writer accepts.
            case ftk::ImageType::L_U8:
            case ftk::ImageType::L_U16:
            case ftk::ImageType::LA_U8:
            case ftk::ImageType::LA_U16:
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
            if (info.video.empty() || !_isCompatible(info.video[0], options))
            {
                throw std::runtime_error(ftk::Format("Unsupported video: \"{0}\"").
                    arg(path.get()));
            }
            return Write::create(path, info, options, _logSystem.lock());
        }

        std::string WritePlugin::getPluginInfo(const IOOptions&) const
        {
            return FTK_VERSION_FULL;
        }
    }
}

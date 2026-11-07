// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/ImageIO.h>

#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>

namespace tl
{
    namespace imageio
    {
        void ReadPlugin::_init(const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            IReadPlugin::_init(
                "ImageIO",
                {
                    { ".heic", FileType::Seq },
                    { ".heif", FileType::Seq }
                },
                logSystem);

            logSystem->print(
                "tl::imageio::ReadPlugin",
                ftk::Format(
                    "\n"
                    "    * Formats: {0}").arg(".heic, .heif"));
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
            return "Apple ImageIO";
        }
    }
}

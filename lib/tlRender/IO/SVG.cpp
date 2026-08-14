// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/SVG.h>

#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>

#include <lunasvg/lunasvg.h>

namespace tl
{
    namespace svg
    {
        void ReadPlugin::_init(const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            IReadPlugin::_init(
                "SVG",
                { { ".svg", FileType::Seq } },
                logSystem);

            logSystem->print(
                "tl::svg::ReadPlugin",
                ftk::Format(
                    "\n"
                    "    * Formats: {0}").arg(".svg"));
        }

        std::shared_ptr<ReadPlugin> ReadPlugin::create(
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            auto out = std::shared_ptr<ReadPlugin>(new ReadPlugin);
            out->_init(logSystem);
            return out;
        }

        std::shared_ptr<IDecode> ReadPlugin::decode(const IOOptions& options)
        {
            return Decode::create(options);
        }

        std::string ReadPlugin::getPluginInfo(const IOOptions&) const
        {
            return LUNASVG_VERSION_STRING;
        }
    }
}

// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/PNG.h>

#include <ftk/Core/Format.h>
#include <ftk/Core/PNG.h>
#include <ftk/Core/Path.h>

namespace tl
{
    namespace png
    {
        void Write::_init(
            const ftk::Path& path,
            const IOInfo& info,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            ISeqWrite::_init(path, info, options, logSystem);
            _plugin.reset(new ftk::png::ImagePlugin);
        }

        Write::Write()
        {}

        Write::~Write()
        {}

        std::shared_ptr<Write> Write::create(
            const ftk::Path& path,
            const IOInfo& info,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            auto out = std::shared_ptr<Write>(new Write);
            out->_init(path, info, options, logSystem);
            return out;
        }

        void Write::_writeVideo(
            const std::string& fileName,
            const OTIO_NS::RationalTime&,
            const std::shared_ptr<ftk::Image>& image,
            const IOOptions& options)
        {
            const auto path = ftk::toFileSystem(fileName);
            const ftk::ImageIOOptions imageOptions(
                options.begin(), options.end());
            const auto& info = image->getInfo();
            if (!_plugin->canWrite(path, info, imageOptions))
            {
                throw std::runtime_error(ftk::Format(
                    "Unsupported video: \"{0}\"").arg(fileName).str());
            }
            _plugin->write(path, info, imageOptions)->write(image);
        }
    }
}

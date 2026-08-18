// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/EXRPrivate.h>

#include <ftk/Core/Format.h>

#include <ImfRgbaFile.h>
#include <ImfStandardAttributes.h>

namespace tl
{
    namespace exr
    {
        void Write::_init(
            const ftk::Path& path,
            const IOInfo& info,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            ISeqWrite::_init(path, info, options, logSystem);

            if (auto i = options.find("OpenEXR/Compression");
                i != options.end())
            {
                from_string(i->second, _compression);
            }
            if (auto i = options.find("OpenEXR/DWACompressionLevel");
                i != options.end())
            {
                std::stringstream ss(i->second);
                ss >> _dwaCompressionLevel;
            }
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
            const IOOptions&)
        {
            const auto& info = image->getInfo();
            Imf::Header header(
                info.size.w,
                info.size.h,
                1.F,
                Imath::V2f(0.F, 0.F),
                1.F,
                Imf::INCREASING_Y,
                toImf(_compression));
            header.dwaCompressionLevel() = _dwaCompressionLevel;
            // The tags the writer was opened with describe the output --
            // e.g., its chromaticities -- and each frame image's own tags
            // go over them.
            ftk::ImageTags tags = _info.tags;
            for (const auto& i : image->getTags())
            {
                tags[i.first] = i.second;
            }
            writeTags(tags, SeqOptions().defaultSpeed, header);
            Imf::RgbaOutputFile f(fileName.c_str(), header);
            const size_t scanlineSize = static_cast<size_t>(info.size.w) * 4 * 2;
            const uint8_t* p = image->getData() + (info.size.h - 1) * scanlineSize;
            f.setFrameBuffer(reinterpret_cast<const Imf::Rgba*>(p), 1, -info.size.w);
            f.writePixels(info.size.h);
        }
    }
}

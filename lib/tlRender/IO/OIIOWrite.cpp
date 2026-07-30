// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/OIIO.h>

#include <ftk/Core/Error.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/Memory.h>
#include <ftk/Core/String.h>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebufalgo.h>

namespace tl
{
    namespace oiio
    {
        void Write::_init(
            const ftk::Path& path,
            const IOInfo& info,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            ISeqWrite::_init(path, info, options, logSystem);
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

        namespace
        {
            OIIO::TypeDesc toOIIO(ftk::ImageType value)
            {
                OIIO::TypeDesc out;
                switch (value)
                {
                case ftk::ImageType::L_U8:
                case ftk::ImageType::LA_U8:
                case ftk::ImageType::RGB_U8:
                case ftk::ImageType::RGBA_U8:
                    out = OIIO::TypeDesc::UINT8;
                    break;
                case ftk::ImageType::L_U16:
                case ftk::ImageType::LA_U16:
                case ftk::ImageType::RGB_U16:
                case ftk::ImageType::RGBA_U16:
                    out = OIIO::TypeDesc::UINT16;
                    break;
                case ftk::ImageType::L_U32:
                case ftk::ImageType::LA_U32:
                case ftk::ImageType::RGB_U32:
                case ftk::ImageType::RGBA_U32:
                    out = OIIO::TypeDesc::UINT32;
                    break;
                case ftk::ImageType::L_F16:
                case ftk::ImageType::LA_F16:
                case ftk::ImageType::RGB_F16:
                case ftk::ImageType::RGBA_F16:
                    out = OIIO::TypeDesc::HALF;
                    break;
                case ftk::ImageType::L_F32:
                case ftk::ImageType::LA_F32:
                case ftk::ImageType::RGB_F32:
                case ftk::ImageType::RGBA_F32:
                    out = OIIO::TypeDesc::FLOAT;
                    break;
                default: break;
                }
                return out;
            }
        }

        void Write::_writeVideo(
            const std::string& fileName,
            const OTIO_NS::RationalTime&,
            const std::shared_ptr<ftk::Image>& image,
            const IOOptions& options)
        {
            // Open the file.
            auto oiioOutput = OIIO::ImageOutput::create(fileName);
            if (!oiioOutput)
            {
                throw std::runtime_error(OIIO::geterror());
            }
            const std::string format = oiioOutput->format_name();

            const auto& info = image->getInfo();
            OIIO::ImageSpec oiioSpec(
                info.size.w,
                info.size.h,
                ftk::getChannelCount(info.type),
                toOIIO(info.type));
            for (const auto& tag : image->getTags())
            {
                oiioSpec.attribute(tag.first, tag.second);
            }
            if ("openexr" == format)
            {
                if (auto i = options.find("OpenEXR/Compression");
                    i != options.end())
                {
                    std::string compression = i->second;
                    if ("dwaa" == compression || "dwab" == compression)
                    {
                        if (auto i = options.find("OpenEXR/DWACompressionLevel");
                            i != options.end())
                        {
                            compression += ":" + i->second;
                        }
                    }
                    oiioSpec.attribute("compression", compression);
                }
            }
            if (!oiioOutput->open(fileName, oiioSpec))
            {
                throw std::runtime_error(OIIO::geterror());
            }

            // Write the image.
            const size_t scanlineByteCount = oiioSpec.scanline_bytes();
            if (!oiioOutput->write_image(
                oiioSpec.format,
                image->getData() + (info.size.h - 1) * scanlineByteCount,
                OIIO::AutoStride,
                -scanlineByteCount,
                OIIO::AutoStride))
            {
                throw std::runtime_error(OIIO::geterror());
            }
        }
    }
}

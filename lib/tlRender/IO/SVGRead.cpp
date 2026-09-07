// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/SVG.h>

#include <ftk/Core/FileIO.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/Path.h>

#include <lunasvg/lunasvg.h>

#include <cmath>
#include <cstring>

namespace tl
{
    namespace svg
    {
        namespace
        {
            ftk::Size2I requestedSize(const IOOptions& options)
            {
                ftk::Size2I out(0, 0);
                if (const auto i = options.find("SVG/Width"); i != options.end())
                {
                    out.w = std::atoi(i->second.c_str());
                }
                if (const auto i = options.find("SVG/Height"); i != options.end())
                {
                    out.h = std::atoi(i->second.c_str());
                }
                return out;
            }

            std::unique_ptr<lunasvg::Document> load(
                const std::string& fileName,
                const ftk::MemFile* memory)
            {
                const auto path = ftk::toFileSystem(fileName);
                auto fileIO = memory ?
                    ftk::FileIO::create(path, *memory) :
                    ftk::FileIO::create(path, ftk::FileMode::Read);
                auto out = lunasvg::Document::loadFromData(ftk::read(fileIO));
                if (!out)
                {
                    throw std::runtime_error(ftk::Format(
                        "Cannot read file: \"{0}\"").arg(fileName).str());
                }
                return out;
            }

            //! The size to rasterize at. A requested width or height on its
            //! own keeps the document's aspect ratio, so that scaling one
            //! dimension does not quietly stretch the drawing.
            ftk::Size2I renderSize(
                const lunasvg::Document& doc,
                const ftk::Size2I& requested,
                const std::string& fileName)
            {
                const float w = doc.width();
                const float h = doc.height();
                if (w <= 0.F || h <= 0.F)
                {
                    throw std::runtime_error(ftk::Format(
                        "Cannot get the size: \"{0}\"").arg(fileName).str());
                }
                ftk::Size2I out(
                    static_cast<int>(std::lround(w)),
                    static_cast<int>(std::lround(h)));
                if (requested.w > 0 && requested.h > 0)
                {
                    out = requested;
                }
                else if (requested.w > 0)
                {
                    out.w = requested.w;
                    out.h = std::max(
                        static_cast<int>(std::lround(requested.w * h / w)), 1);
                }
                else if (requested.h > 0)
                {
                    out.w = std::max(
                        static_cast<int>(std::lround(requested.h * w / h)), 1);
                    out.h = requested.h;
                }
                return out;
            }

            ftk::ImageInfo imageInfo(const ftk::Size2I& size)
            {
                ftk::ImageInfo out(size, ftk::ImageType::RGBA_U8);
                // lunasvg gives the top row first.
                out.layout.mirror.y = true;
                return out;
            }
        }

        Decode::Decode(const IOOptions& options) :
            _requestedSize(requestedSize(options))
        {}

        Decode::~Decode()
        {}

        std::shared_ptr<Decode> Decode::create(const IOOptions& options)
        {
            return std::shared_ptr<Decode>(new Decode(options));
        }

        IOInfo Decode::getInfo(
            const std::string& fileName,
            const ftk::MemFile* memory)
        {
            auto doc = load(fileName, memory);
            IOInfo out;
            out.video.push_back(
                imageInfo(renderSize(*doc, _requestedSize, fileName)));
            return out;
        }

        VideoData Decode::readVideo(
            const std::string& fileName,
            const ftk::MemFile* memory,
            const OTIO_NS::RationalTime& time,
            const IOOptions&)
        {
            auto doc = load(fileName, memory);
            const ftk::Size2I size = renderSize(*doc, _requestedSize, fileName);

            auto bitmap = doc->renderToBitmap(size.w, size.h);
            if (bitmap.isNull())
            {
                throw std::runtime_error(ftk::Format(
                    "Cannot render file: \"{0}\"").arg(fileName).str());
            }
            // lunasvg rasterizes to premultiplied ARGB32.
            bitmap.convertToRGBA();

            VideoData out;
            out.time = time;
            out.image = ftk::Image::create(imageInfo(size));
            const size_t rowByteCount = static_cast<size_t>(size.w) * 4;
            for (int y = 0; y < size.h; ++y)
            {
                std::memcpy(
                    out.image->getData() + y * rowByteCount,
                    bitmap.data() + y * bitmap.stride(),
                    rowByteCount);
            }
            return out;
        }
    }
}

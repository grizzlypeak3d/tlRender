// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UI/ThumbnailColors.h>

#include <tlRender/UI/ThumbnailSystem.h>

#include <tlRender/Timeline/CPUColor.h>

#include <cstring>
#include <map>

namespace tl
{
    namespace ui
    {
        struct ThumbnailColors::Private
        {
            OCIOOptions ocio;
            LUTOptions lut;
            std::function<std::string(const std::string&, const ftk::ImageTags&)> ocioInputResolver;
#if defined(TLRENDER_OCIO)
            //! One per input color space; one that cannot be had is kept
            //! empty, so it is not tried for every thumbnail.
            std::map<std::string, std::shared_ptr<CPUColorTable> > colors;
#endif // TLRENDER_OCIO
            std::map<std::shared_ptr<ftk::Image>, std::shared_ptr<ftk::Image> > thumbnails;
        };

        ThumbnailColors::ThumbnailColors() :
            _p(new Private)
        {}

        ThumbnailColors::~ThumbnailColors()
        {}

        void ThumbnailColors::setOCIOOptions(const OCIOOptions& value)
        {
            FTK_P();
            if (value == p.ocio)
                return;
            p.ocio = value;
            _clear();
        }

        void ThumbnailColors::setLUTOptions(const LUTOptions& value)
        {
            FTK_P();
            if (value == p.lut)
                return;
            p.lut = value;
            _clear();
        }

        void ThumbnailColors::setOCIOInputResolver(
            const std::function<std::string(
                const std::string& path,
                const ftk::ImageTags&)>& value)
        {
            FTK_P();
            p.ocioInputResolver = value;
            _clear();
        }

        std::shared_ptr<ftk::Image> ThumbnailColors::get(
            const std::shared_ptr<ftk::Image>& image)
        {
#if defined(TLRENDER_OCIO)
            FTK_P();
            const bool lutOn = p.lut.enabled && !p.lut.fileName.empty();
            if (!image ||
                (!p.ocio.enabled && !lutOn) ||
                image->getInfo().type != ftk::ImageType::RGBA_U8)
                return image;
            const auto i = p.thumbnails.find(image);
            if (i != p.thumbnails.end())
                return i->second;

            // The media's own input, as the picture is drawn with, or the
            // options' when it has none: a compared timeline or a clip of
            // an OTIO file can be in another color space from the file the
            // options were resolved for.
            std::string input;
            if (p.ocio.enabled)
            {
                if (p.ocioInputResolver)
                {
                    const ftk::ImageTags& tags = image->getTags();
                    const auto j = tags.find(thumbnailPathTag);
                    if (j != tags.end() && !j->second.empty())
                    {
                        input = p.ocioInputResolver(j->second, tags);
                    }
                }
                if (input.empty())
                {
                    input = p.ocio.input;
                }
            }

            // As the picture is drawn, too: with no input there is nothing
            // for the configuration to do. Thumbnails have no color
            // corrections; a Display P3 display is taken into sRGB, as the
            // picture is for a window.
            std::shared_ptr<ftk::Image> out = image;
            const bool ocioOn = p.ocio.enabled && !input.empty();
            if (ocioOn || lutOn)
            {
                std::shared_ptr<CPUColorTable> color;
                const auto j = p.colors.find(input);
                if (j != p.colors.end())
                {
                    color = j->second;
                }
                else
                {
                    OCIOOptions options = p.ocio;
                    options.enabled = ocioOn;
                    options.input = input;
                    try
                    {
                        color = std::make_shared<CPUColorTable>(CPUColor(
                            options,
                            p.lut,
                            DisplayOptions(),
                            true));
                    }
                    catch (const std::exception&)
                    {}
                    p.colors[input] = color;
                }
                if (color)
                {
                    const ftk::ImageInfo& info = image->getInfo();
                    out = ftk::Image::create(info);
                    out->setTags(image->getTags());
                    std::memcpy(out->getData(), image->getData(), image->getByteCount());
                    // Rows can be padded to the image's alignment.
                    const size_t h = info.size.h;
                    const size_t stride = h > 0 ? image->getByteCount() / h : 0;
                    for (size_t y = 0; y < h; ++y)
                    {
                        color->apply(out->getData() + y * stride, info.size.w);
                    }
                }
            }
            p.thumbnails[image] = out;
            return out;
#else // TLRENDER_OCIO
            return image;
#endif // TLRENDER_OCIO
        }

        void ThumbnailColors::prune()
        {
            FTK_P();
            for (auto i = p.thumbnails.begin(); i != p.thumbnails.end();)
            {
                if (1 == i->first.use_count())
                {
                    i = p.thumbnails.erase(i);
                }
                else
                {
                    ++i;
                }
            }
        }

        void ThumbnailColors::_clear()
        {
            FTK_P();
#if defined(TLRENDER_OCIO)
            p.colors.clear();
#endif // TLRENDER_OCIO
            p.thumbnails.clear();
        }
    }
}

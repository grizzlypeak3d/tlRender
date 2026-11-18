// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UITest/ThumbnailColorsTest.h>

#include <tlRender/UI/ThumbnailColors.h>
#include <tlRender/UI/ThumbnailSystem.h>

#include <ftk/Core/Assert.h>

#include <cstring>

#if defined(TLRENDER_OCIO)
#include <OpenColorIO/OpenColorIO.h>
#endif // TLRENDER_OCIO

namespace tl
{
    namespace ui_tests
    {
        ThumbnailColorsTest::ThumbnailColorsTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "ui_tests::ThumbnailColorsTest")
        {}

        std::shared_ptr<ThumbnailColorsTest> ThumbnailColorsTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<ThumbnailColorsTest>(new ThumbnailColorsTest(context));
        }

        namespace
        {
            std::shared_ptr<ftk::Image> thumbnail(
                const std::string& path,
                ftk::ImageType type = ftk::ImageType::RGBA_U8)
            {
                auto out = ftk::Image::create(16, 8, type);
                // A ramp, so that the color has something to change.
                for (size_t i = 0; i < out->getByteCount(); ++i)
                {
                    out->getData()[i] = static_cast<uint8_t>(i * 7);
                }
                ftk::ImageTags tags;
                tags["Camera"] = "A";
                if (!path.empty())
                {
                    tags[ui::thumbnailPathTag] = path;
                }
                out->setTags(tags);
                return out;
            }

            bool same(
                const std::shared_ptr<ftk::Image>& a,
                const std::shared_ptr<ftk::Image>& b)
            {
                return a->getByteCount() == b->getByteCount() &&
                    0 == std::memcmp(a->getData(), b->getData(), a->getByteCount());
            }
        }

        void ThumbnailColorsTest::run()
        {
#if defined(TLRENDER_OCIO)
            namespace OCIO = OCIO_NAMESPACE;
            auto config = OCIO::Config::CreateFromFile("ocio://default");
            OCIOOptions ocio;
            ocio.enabled = true;
            ocio.display = config->getDefaultDisplay();
            ocio.view = config->getDefaultView(ocio.display.c_str());

            {
                // With the color off, or nothing to color, a thumbnail is
                // its own.
                ui::ThumbnailColors colors;
                FTK_CHECK(!colors.get(nullptr));
                auto a = thumbnail("a.mov");
                FTK_CHECK(a == colors.get(a));
                auto ocioOff = ocio;
                ocioOff.enabled = false;
                ocioOff.input = "ACEScct";
                colors.setOCIOOptions(ocioOff);
                FTK_CHECK(a == colors.get(a));

                // On with no input, nothing for the configuration to do.
                colors.setOCIOOptions(ocio);
                FTK_CHECK(a == colors.get(a));
            }
            {
                // With an input, a thumbnail in color, kept, with the
                // thumbnail's tags; another type is not colored.
                ui::ThumbnailColors colors;
                auto options = ocio;
                options.input = "ACEScct";
                colors.setOCIOOptions(options);
                auto a = thumbnail("a.mov");
                auto aColor = colors.get(a);
                FTK_CHECK(aColor && aColor != a);
                FTK_CHECK(aColor->getInfo() == a->getInfo());
                FTK_CHECK(aColor->getTags() == a->getTags());
                FTK_CHECK(!same(aColor, a));
                FTK_CHECK(aColor == colors.get(a));
                auto rgb = thumbnail("a.mov", ftk::ImageType::RGB_U8);
                FTK_CHECK(rgb == colors.get(rgb));

                // New options make it again.
                std::weak_ptr<ftk::Image> aColorWeak = aColor;
                aColor.reset();
                options.input = "ACEScg";
                colors.setOCIOOptions(options);
                FTK_CHECK(aColorWeak.expired());
                aColor = colors.get(a);
                FTK_CHECK(aColor != a);

                // Let go of once nothing else holds the thumbnail.
                aColorWeak = aColor;
                aColor.reset();
                colors.prune();
                FTK_CHECK(!aColorWeak.expired());
                a.reset();
                colors.prune();
                FTK_CHECK(aColorWeak.expired());
            }
            {
                // Each thumbnail with its own media's input, and the
                // options' own when the resolver has none.
                ui::ThumbnailColors colors;
                colors.setOCIOOptions(ocio);
                std::vector<std::string> asked;
                colors.setOCIOInputResolver(
                    [&asked](const std::string& path, const ftk::ImageTags& tags)
                    {
                        // With the thumbnail's tags, for what the file says.
                        const auto i = tags.find("Camera");
                        asked.push_back(path + (i != tags.end() ? i->second : std::string()));
                        return "a.mov" == path ? std::string("ACEScct") : std::string();
                    });
                auto a = thumbnail("a.mov");
                auto b = thumbnail("b.mov");
                auto c = thumbnail(std::string());
                FTK_CHECK(a != colors.get(a));
                FTK_CHECK(b == colors.get(b));
                FTK_CHECK(c == colors.get(c));
                FTK_CHECK(std::vector<std::string>({ "a.movA", "b.movA" }) == asked);

                auto options = ocio;
                options.input = "ACEScg";
                colors.setOCIOOptions(options);
                auto aColor = colors.get(a);
                auto bColor = colors.get(b);
                FTK_CHECK(aColor != a && bColor != b);
                // The same pixels in different color spaces.
                FTK_CHECK(!same(aColor, bColor));
            }
#endif // TLRENDER_OCIO
        }
    }
}

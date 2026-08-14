// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOTest/SVGTest.h>

#include <tlRender/IO/SVG.h>
#include <tlRender/IO/System.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/FileIO.h>
#include <ftk/Core/Format.h>

namespace tl
{
    namespace io_tests
    {
        namespace
        {
            // A 40x20 document, so a stretched aspect ratio would show.
            const std::string svg =
                "<svg xmlns=\"http://www.w3.org/2000/svg\" "
                "width=\"40\" height=\"20\" viewBox=\"0 0 40 20\">"
                "<rect width=\"40\" height=\"20\" fill=\"#ff0000\"/>"
                "</svg>";
        }

        SVGTest::SVGTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "io_tests::SVGTest")
        {}

        std::shared_ptr<SVGTest> SVGTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<SVGTest>(new SVGTest(context));
        }

        ftk::Path SVGTest::_write(
            const std::string& fileName,
            const std::string& text)
        {
            const ftk::Path out((_getTempDir() / fileName).u8string());
            auto fileIO = ftk::FileIO::create(out.get(), ftk::FileMode::Write);
            fileIO->write(text.c_str(), text.size());
            return out;
        }

        std::shared_ptr<ftk::Image> SVGTest::_read(
            const std::shared_ptr<IReadPlugin>& plugin,
            const ftk::Path& path,
            bool memoryIO,
            const IOOptions& options)
        {
            std::vector<uint8_t> memoryData;
            std::vector<ftk::MemFile> memory;
            if (memoryIO)
            {
                auto fileIO = ftk::FileIO::create(path.get(), ftk::FileMode::Read);
                memoryData.resize(fileIO->getSize());
                fileIO->read(memoryData.data(), memoryData.size());
                memory.push_back(ftk::MemFile(nullptr, memoryData.data(), memoryData.size()));
            }
            const ftk::MemFile* mem = !memory.empty() ? &memory[0] : nullptr;
            auto decode = plugin->decode(options);
            FTK_CHECK(decode);

            const auto info = decode->getInfo(path.get(), mem);
            FTK_CHECK(!info.video.empty());
            const auto videoData = decode->readVideo(
                path.get(), mem, OTIO_NS::RationalTime(0.0, 24.0), options);
            FTK_CHECK(videoData.image);
            // The information is asked for without options, so it can only
            // match the image if the decoder settled the size when it was
            // made; that is the whole reason it does.
            FTK_CHECK(videoData.image->getSize() == info.video[0].size);
            return videoData.image;
        }

        void SVGTest::run()
        {
            auto readSystem = _context->getSystem<ReadSystem>();
            auto plugin = readSystem->getPlugin<svg::ReadPlugin>();
            FTK_CHECK(plugin);
            _print(ftk::Format("lunasvg: {0}").arg(plugin->getPluginInfo()));

            // The plugin claims the extension, so a .svg opens through it
            // rather than through whatever else was built in.
            FTK_CHECK(readSystem->getPlugin(ftk::Path("test.svg")) == plugin);

            const ftk::Path path = _write("SVGTest.svg", svg);
            for (const bool memoryIO : { false, true })
            {
                // The document's own size, when nothing asks otherwise.
                auto image = _read(plugin, path, memoryIO, {});
                FTK_CHECK(ftk::Size2I(40, 20) == image->getSize());
                FTK_CHECK(ftk::ImageType::RGBA_U8 == image->getType());

                // A width on its own keeps the aspect ratio.
                image = _read(plugin, path, memoryIO, { { "SVG/Width", "80" } });
                FTK_CHECK(ftk::Size2I(80, 40) == image->getSize());

                // As does a height on its own.
                image = _read(plugin, path, memoryIO, { { "SVG/Height", "5" } });
                FTK_CHECK(ftk::Size2I(10, 5) == image->getSize());

                // Both together are taken as given, aspect ratio or not.
                image = _read(
                    plugin,
                    path,
                    memoryIO,
                    { { "SVG/Width", "13" }, { "SVG/Height", "70" } });
                FTK_CHECK(ftk::Size2I(13, 70) == image->getSize());

                // The rectangle covers the document, so every pixel is
                // opaque red; this is what says the rows and the channels
                // did not get turned around.
                image = _read(plugin, path, memoryIO, {});
                const uint8_t* p = image->getData();
                for (size_t i = 0; i < 40 * 20; ++i, p += 4)
                {
                    FTK_CHECK(255 == p[0] && 0 == p[1] && 0 == p[2] && 255 == p[3]);
                }
            }

            // A file that is not an SVG, and one that is not there.
            const ftk::Path badPath = _write("SVGTestBad.svg", "not an svg");
            try
            {
                auto decode = plugin->decode();
                decode->getInfo(badPath.get());
                FTK_CHECK(false);
            }
            catch (const std::exception&)
            {}
            try
            {
                auto decode = plugin->decode();
                decode->getInfo(
                    (_getTempDir() / "SVGTestMissing.svg").u8string());
                FTK_CHECK(false);
            }
            catch (const std::exception&)
            {}
        }
    }
}

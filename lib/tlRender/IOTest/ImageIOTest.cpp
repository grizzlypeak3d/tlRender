// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOTest/ImageIOTest.h>

#include <tlRender/IO/ImageIO.h>
#include <tlRender/IO/System.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/FileIO.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/Path.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

namespace tl
{
    namespace io_tests
    {
        namespace
        {
            bool canWriteHEIC()
            {
                bool out = false;
                if (CFArrayRef types = CGImageDestinationCopyTypeIdentifiers())
                {
                    out = CFArrayContainsValue(
                        types, CFRangeMake(0, CFArrayGetCount(types)), CFSTR("public.heic"));
                    CFRelease(types);
                }
                return out;
            }

            //! A 40x20 HEIC, red on the left and blue on the right, with
            //! the orientation given.
            bool writeHEIC(const ftk::Path& path, int orientation)
            {
                bool out = false;
                CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
                CGContextRef context = CGBitmapContextCreate(
                    nullptr, 40, 20, 8, 40 * 4, colorSpace,
                    static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast));
                CGContextSetRGBFillColor(context, 1.0, 0.0, 0.0, 1.0);
                CGContextFillRect(context, CGRectMake(0, 0, 20, 20));
                CGContextSetRGBFillColor(context, 0.0, 0.0, 1.0, 1.0);
                CGContextFillRect(context, CGRectMake(20, 0, 20, 20));
                CGImageRef image = CGBitmapContextCreateImage(context);

                const std::string fileName = path.get();
                CFURLRef url = CFURLCreateFromFileSystemRepresentation(
                    nullptr,
                    reinterpret_cast<const UInt8*>(fileName.c_str()),
                    fileName.size(),
                    false);
                CGImageDestinationRef destination = CGImageDestinationCreateWithURL(
                    url, CFSTR("public.heic"), 1, nullptr);
                if (destination)
                {
                    const float quality = 1.F;
                    CFNumberRef qualityNumber = CFNumberCreate(nullptr, kCFNumberFloatType, &quality);
                    CFNumberRef orientationNumber = CFNumberCreate(nullptr, kCFNumberIntType, &orientation);
                    const void* keys[] =
                    {
                        kCGImageDestinationLossyCompressionQuality,
                        kCGImagePropertyOrientation
                    };
                    const void* values[] = { qualityNumber, orientationNumber };
                    CFDictionaryRef properties = CFDictionaryCreate(
                        nullptr, keys, values, 2,
                        &kCFTypeDictionaryKeyCallBacks,
                        &kCFTypeDictionaryValueCallBacks);
                    CGImageDestinationAddImage(destination, image, properties);
                    out = CGImageDestinationFinalize(destination);
                    CFRelease(properties);
                    CFRelease(orientationNumber);
                    CFRelease(qualityNumber);
                    CFRelease(destination);
                }
                CFRelease(url);
                CGImageRelease(image);
                CGContextRelease(context);
                CGColorSpaceRelease(colorSpace);
                return out;
            }

            //! Whether a pixel is near red or near blue: HEIC is lossy.
            bool isRed(const uint8_t* p)
            {
                return p[0] > 200 && p[1] < 60 && p[2] < 60 && 255 == p[3];
            }

            bool isBlue(const uint8_t* p)
            {
                return p[0] < 60 && p[1] < 60 && p[2] > 200 && 255 == p[3];
            }
        }

        ImageIOTest::ImageIOTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "io_tests::ImageIOTest")
        {}

        std::shared_ptr<ImageIOTest> ImageIOTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<ImageIOTest>(new ImageIOTest(context));
        }

        std::shared_ptr<ftk::Image> ImageIOTest::_read(
            const std::shared_ptr<IReadPlugin>& plugin,
            const ftk::Path& path,
            bool memoryIO)
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
            auto decode = plugin->decode();
            FTK_CHECK(decode);
            const auto info = decode->getInfo(path.get(), mem);
            FTK_CHECK(!info.video.empty());
            const auto videoData = decode->readVideo(
                path.get(), mem, OTIO_NS::RationalTime(0.0, 24.0));
            FTK_CHECK(videoData.image);
            FTK_CHECK(videoData.image->getInfo() == info.video[0]);
            return videoData.image;
        }

        void ImageIOTest::run()
        {
            auto readSystem = _context->getSystem<ReadSystem>();
            auto plugin = readSystem->getPlugin<imageio::ReadPlugin>();
            FTK_CHECK(plugin);
            _print(ftk::Format("ImageIO: {0}").arg(plugin->getPluginInfo()));

            // The plugin claims the extensions, whatever else was built in.
            FTK_CHECK(readSystem->getPlugin(ftk::Path("test.heic")) == plugin);
            FTK_CHECK(readSystem->getPlugin(ftk::Path("TEST.HEIF")) == plugin);

            if (!canWriteHEIC())
            {
                _print("This machine cannot write HEIC; skipping the reads");
            }
            else
            {
                // Upright: red on the left, blue on the right. The rows
                // are the top first.
                const ftk::Path upright(ftk::fromFileSystem(_getTempDir() / "ImageIOTest.heic"));
                FTK_CHECK(writeHEIC(upright, 1));
                // Turned a quarter clockwise to be shown (orientation 6):
                // the stored left side is the top.
                const ftk::Path turned(ftk::fromFileSystem(_getTempDir() / "ImageIOTestTurned.heic"));
                FTK_CHECK(writeHEIC(turned, 6));
                for (const bool memoryIO : { false, true })
                {
                    auto image = _read(plugin, upright, memoryIO);
                    FTK_CHECK(ftk::Size2I(40, 20) == image->getSize());
                    FTK_CHECK(ftk::ImageType::RGBA_U8 == image->getType());
                    FTK_CHECK(image->getInfo().layout.mirror.y);
                    const uint8_t* p = image->getData();
                    FTK_CHECK(isRed(p + (10 * 40 + 5) * 4));
                    FTK_CHECK(isBlue(p + (10 * 40 + 35) * 4));

                    image = _read(plugin, turned, memoryIO);
                    FTK_CHECK(ftk::Size2I(20, 40) == image->getSize());
                    p = image->getData();
                    FTK_CHECK(isRed(p + (5 * 20 + 10) * 4));
                    FTK_CHECK(isBlue(p + (35 * 20 + 10) * 4));
                }
            }

            // A file that is not an image, and one that is not there.
            const ftk::Path badPath(ftk::fromFileSystem(_getTempDir() / "ImageIOTestBad.heic"));
            {
                auto fileIO = ftk::FileIO::create(badPath.get(), ftk::FileMode::Write);
                const std::string text = "not an image";
                fileIO->write(text.c_str(), text.size());
            }
            try
            {
                plugin->decode()->getInfo(badPath.get());
                FTK_CHECK(false);
            }
            catch (const std::exception&)
            {}
            try
            {
                plugin->decode()->getInfo(
                    ftk::fromFileSystem(_getTempDir() / "ImageIOTestMissing.heic"));
                FTK_CHECK(false);
            }
            catch (const std::exception&)
            {}
        }
    }
}

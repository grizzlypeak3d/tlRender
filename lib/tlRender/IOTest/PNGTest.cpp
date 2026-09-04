// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOTest/PNGTest.h>

#include <tlRender/IO/PNG.h>
#include <tlRender/IO/System.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/FileIO.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/Path.h>

#include <cstring>

namespace tl
{
    namespace io_tests
    {
        namespace
        {
            const ftk::Size2I size(17, 9);

            //! A pattern that differs along both axes and between channels,
            //! so that a flipped row order, a swapped channel or the wrong
            //! bit depth all change the bytes.
            void fill(const std::shared_ptr<ftk::Image>& image)
            {
                uint8_t* data = image->getData();
                const size_t byteCount = image->getByteCount();
                for (size_t i = 0; i < byteCount; ++i)
                {
                    data[i] = static_cast<uint8_t>((i * 37 + 11) % 251);
                }
            }
        }

        PNGTest::PNGTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "io_tests::PNGTest")
        {}

        std::shared_ptr<PNGTest> PNGTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<PNGTest>(new PNGTest(context));
        }

        void PNGTest::run()
        {
            _plugins();
            _roundTrip();
            _writeInfo();
        }

        void PNGTest::_plugins()
        {
            // The point of the plugin: the codec comes from feather-tk, so
            // these are there in every configuration, including the one with
            // none of the optional media libraries.
            auto readPlugin = _context->getSystem<ReadSystem>()->
                getPlugin<png::ReadPlugin>();
            auto writePlugin = _context->getSystem<WriteSystem>()->
                getPlugin<png::WritePlugin>();
            FTK_CHECK(readPlugin);
            FTK_CHECK(writePlugin);
            _print(ftk::Format("feather-tk: {0}").arg(readPlugin->getPluginInfo()));

            FTK_CHECK(readPlugin->getExts() == std::set<std::string>({ ".png" }));
            FTK_CHECK(writePlugin->getExts() == std::set<std::string>({ ".png" }));
        }

        void PNGTest::_roundTrip()
        {
            auto readPlugin = _context->getSystem<ReadSystem>()->
                getPlugin<png::ReadPlugin>();
            auto writePlugin = _context->getSystem<WriteSystem>()->
                getPlugin<png::WritePlugin>();
            auto decode = readPlugin->decode();
            FTK_CHECK(decode);

            size_t count = 0;
            for (const ftk::ImageType type : {
                ftk::ImageType::L_U8,
                ftk::ImageType::L_U16,
                ftk::ImageType::LA_U8,
                ftk::ImageType::LA_U16,
                ftk::ImageType::RGB_U8,
                ftk::ImageType::RGB_U16,
                ftk::ImageType::RGBA_U8,
                ftk::ImageType::RGBA_U16 })
            {
                const ftk::ImageInfo imageInfo(size, type);

                // Every one of these is stored as it is, rather than being
                // widened to something PNG can hold.
                FTK_CHECK(writePlugin->getInfo(imageInfo).type == type);

                auto image = ftk::Image::create(imageInfo);
                fill(image);

                // Lettered rather than named after the type or numbered:
                // the type names have spaces in them, and a name ending in a
                // digit is a frame of a sequence, which is what the writer
                // would then write instead.
                const std::string letter(1, 'a' + static_cast<char>(count++));
                const ftk::Path path(ftk::fromFileSystem(_getTempDir() /
                    ftk::Format("PNGTest_{0}.png").arg(letter).str()));
                IOInfo info;
                info.video.push_back(imageInfo);
                const OTIO_NS::RationalTime time(0.0, 24.0);
                writePlugin->write(path, info)->writeVideo(time, image);

                auto fileIO = ftk::FileIO::create(path.get(), ftk::FileMode::Read);
                std::vector<uint8_t> bytes(fileIO->getSize());
                fileIO->read(bytes.data(), bytes.size());
                const ftk::MemFile memFile(nullptr, bytes.data(), bytes.size());

                const IOInfo readInfo = decode->getInfo(path.get());
                FTK_CHECK(!readInfo.video.empty());
                FTK_CHECK(readInfo.video[0].size == size);
                FTK_CHECK(readInfo.video[0].type == type);

                // A PNG holds its rows top down, and the writer takes the
                // last row of the image first. So the file preserves the
                // picture while the buffer comes back the other way up, and
                // the flag on the way out is what says so.
                FTK_CHECK(readInfo.video[0].layout.mirror.y);

                for (const ftk::MemFile* memory : {
                    static_cast<const ftk::MemFile*>(nullptr), &memFile })
                {
                    const auto videoData = decode->readVideo(
                        path.get(), memory, time);
                    FTK_CHECK(videoData.image);
                    FTK_CHECK(videoData.image->getType() == type);
                    FTK_CHECK(videoData.image->getSize() == size);

                    const size_t byteCount = image->getByteCount();
                    FTK_CHECK(videoData.image->getByteCount() == byteCount);
                    const size_t rowByteCount = byteCount / size.h;
                    for (int y = 0; y < size.h; ++y)
                    {
                        FTK_CHECK(0 == memcmp(
                            videoData.image->getData() + y * rowByteCount,
                            image->getData() + (size.h - 1 - y) * rowByteCount,
                            rowByteCount));
                    }
                }

                _print(ftk::Format("{0}: matches what was written").arg(type));
            }
        }

        void PNGTest::_writeInfo()
        {
            auto writePlugin = _context->getSystem<WriteSystem>()->
                getPlugin<png::WritePlugin>();

            // Floating point and the wider integers are not PNG; the writer
            // offers what it can store instead of refusing.
            for (const ftk::ImageType type : {
                ftk::ImageType::L_F32,
                ftk::ImageType::RGB_U32,
                ftk::ImageType::RGBA_F16,
                ftk::ImageType::YUV_420P_U8 })
            {
                const ftk::ImageInfo out =
                    writePlugin->getInfo(ftk::ImageInfo(size, type));
                FTK_CHECK(out.type == ftk::ImageType::RGBA_U8);
                FTK_CHECK(out.size == size);
            }
        }
    }
}

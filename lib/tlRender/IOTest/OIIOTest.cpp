// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOTest/OIIOTest.h>

#include <tlRender/IO/OIIO.h>
#include <tlRender/IO/System.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/Path.h>

#include <sstream>

namespace tl
{
    namespace io_tests
    {
        OIIOTest::OIIOTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "io_tests::OIIOTest")
        {}

        std::shared_ptr<OIIOTest> OIIOTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<OIIOTest>(new OIIOTest(context));
        }

        void OIIOTest::write(
            const std::shared_ptr<IWritePlugin>& plugin,
            const std::shared_ptr<ftk::Image>& image,
            const ftk::Path& path,
            const ftk::ImageInfo& imageInfo,
            const IOOptions& options)
        {
            IOInfo info;
            info.video.push_back(imageInfo);
            info.videoTime = OTIO_NS::TimeRange(OTIO_NS::RationalTime(0.0, 24.0), OTIO_NS::RationalTime(1.0, 24.0));
            auto write = plugin->write(path, info, options);
            write->writeVideo(OTIO_NS::RationalTime(0.0, 24.0), image);
        }

        void OIIOTest::read(
            const std::shared_ptr<IReadPlugin>& plugin,
            const std::shared_ptr<ftk::Image>& image,
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
            // A sequence has no reader: one file is one decode.
            auto decode = plugin->decode(options);
            FTK_CHECK(decode);
            const ftk::MemFile* mem = !memory.empty() ? &memory[0] : nullptr;
            const auto ioInfo = decode->getInfo(path.get(), mem);
            FTK_CHECK(!ioInfo.video.empty());
            const auto videoData = decode->readVideo(
                path.get(), mem, OTIO_NS::RationalTime(0.0, 24.0), options);
            FTK_CHECK(videoData.image);
            FTK_CHECK(videoData.image->getSize() == image->getSize());
            //! \todo Compare image data.
            //FTK_CHECK(0 == memcmp(
            //    videoData.image->getData(),
            //    image->getData(),
            //    image->getDataByteCount()));
        }

        void OIIOTest::readError(
            const std::shared_ptr<IReadPlugin>& plugin,
            const std::shared_ptr<ftk::Image>& image,
            const ftk::Path& path,
            bool memoryIO,
            const IOOptions& options)
        {
            {
                auto fileIO = ftk::FileIO::create(path.get(), ftk::FileMode::Read);
                const size_t size = fileIO->getSize();
                fileIO.reset();
                ftk::truncateFile(path.get(), size / 2);
            }
            std::vector<uint8_t> memoryData;
            std::vector<ftk::MemFile> memory;
            if (memoryIO)
            {
                auto fileIO = ftk::FileIO::create(path.get(), ftk::FileMode::Read);
                memoryData.resize(fileIO->getSize());
                fileIO->read(memoryData.data(), memoryData.size());
                memory.push_back(ftk::MemFile(nullptr, memoryData.data(), memoryData.size()));
            }
            // The file has been truncated, so this is expected to fail;
            // a reader swallowed that, a decoder reports it.
            try
            {
                auto decode = plugin->decode(options);
                decode->readVideo(
                    path.get(),
                    !memory.empty() ? &memory[0] : nullptr,
                    OTIO_NS::RationalTime(0.0, 24.0),
                    options);
            }
            catch (const std::exception&)
            {}
        }

        void OIIOTest::run()
        {
            auto readSystem = _context->getSystem<ReadSystem>();
            auto readPlugin = readSystem->getPlugin<oiio::ReadPlugin>();
            auto writeSystem = _context->getSystem<WriteSystem>();
            auto writePlugin = writeSystem->getPlugin<oiio::WritePlugin>();

            const std::vector<std::string> fileNames =
            {
                "OIIOTest",
                "大平原"
            };
            const std::vector<std::string> extensions =
            {
                ".png",
                //".exr"
            };
            const std::vector<bool> memoryIOList =
            {
                false,
                true
            };
            const std::vector<ftk::Size2I> sizes =
            {
                ftk::Size2I(16, 16),
                ftk::Size2I(1, 1),
                ftk::Size2I(0, 0)
            };
            const std::vector<IOOptions> optionsList =
            {
                {},
                /*{
                    { "OpenEXR/Compression", "none" }
                },
                {
                    { "OpenEXR/Compression", "zip" }
                },
                {
                    { "OpenEXR/Compression", "dwaa" },
                    { "OpenEXR/DWACompressionLevel", "50" },
                }*/
            };
            size_t count = 0;
            for (const auto& fileName : fileNames)
            {
                for (const auto& extension : extensions)
                {
                    for (const bool memoryIO : memoryIOList)
                    {
                        for (const auto& size : sizes)
                        {
                            for (const auto pixelType : ftk::getImageTypeEnums())
                            {
                                for (const auto& options : optionsList)
                                {
                                    const auto imageInfo = writePlugin->getInfo(ftk::ImageInfo(size, pixelType));
                                    if (imageInfo.isValid())
                                    {
                                        ftk::Path path;
                                        {
                                            std::stringstream ss;
                                            ss << fileName << ' ' << count << ' ' << size << ' ' << pixelType << ".0" << extension;
                                            _print(ss.str());
                                            path = ftk::Path(ftk::fromFileSystem(_getTempDir() / ss.str()));
                                        }
                                        const auto image = ftk::Image::create(imageInfo);
                                        image->zero();
                                        try
                                        {
                                            write(writePlugin, image, path, imageInfo, options);
                                            read(readPlugin, image, path, memoryIO, options);
                                            readError(readPlugin, image, path, memoryIO, options);
                                        }
                                        catch (const std::exception& e)
                                        {
                                            _error(e.what());
                                        }

                                        ++count;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

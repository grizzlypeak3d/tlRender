// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOTest/IOTest.h>

#include <tlRender/IO/System.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/Image.h>
#include <ftk/Core/String.h>

#include <atomic>
#include <cstring>
#include <sstream>
#include <thread>

namespace tl
{
    namespace io_tests
    {
        IOTest::IOTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "IOTest::IOTest")
        {}

        std::shared_ptr<IOTest> IOTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<IOTest>(new IOTest(context));
        }

        void IOTest::run()
        {
            _videoData();
            _ioSystem();
            _decode();
        }

        void IOTest::_videoData()
        {
            {
                const VideoData v;
                FTK_ASSERT(!isValid(v.time));
                FTK_ASSERT(!v.image);
            }
            {
                const auto time = OTIO_NS::RationalTime(1.0, 24.0);
                const uint16_t layer = 1;
                const auto image = ftk::Image::create(160, 80, ftk::ImageType::L_U8);
                const VideoData v(time, layer, image);
                FTK_ASSERT(time.strictly_equal(v.time));
                FTK_ASSERT(layer == v.layer);
                FTK_ASSERT(image == v.image);
            }
            {
                const auto time = OTIO_NS::RationalTime(1.0, 24.0);
                const uint16_t layer = 1;
                const auto image = ftk::Image::create(16, 16, ftk::ImageType::L_U8);
                const VideoData a(time, layer, image);
                VideoData b(time, layer, image);
                FTK_ASSERT(a == b);
                b.time = OTIO_NS::RationalTime(2.0, 24.0);
                FTK_ASSERT(a != b);
                FTK_ASSERT(a < b);
            }
        }

        namespace
        {
            class DummyReadPlugin : public IReadPlugin
            {
            public:
                std::shared_ptr<IRead> read(
                    const ftk::Path&,
                    const IOOptions& = IOOptions()) override
                {
                    return nullptr;
                }
            };

            class DummyWritePlugin : public IWritePlugin
            {
            public:
                ftk::ImageInfo getInfo(
                    const ftk::ImageInfo&,
                    const IOOptions& = IOOptions()) const override
                {
                    return ftk::ImageInfo();
                }

                std::shared_ptr<IWrite> write(
                    const ftk::Path&,
                    const IOInfo&,
                    const IOOptions& = IOOptions()) override
                {
                    return nullptr;
                }
            };
        }

        void IOTest::_ioSystem()
        {
            auto readSystem = _context->getSystem<ReadSystem>();
            {
                for (const auto& plugin : readSystem->getPlugins())
                {
                    const auto& exts = plugin->getExts();
                    std::stringstream ss;
                    ss << plugin->getPluginName() << ": " <<
                        ftk::join(std::vector<std::string>(exts.begin(), exts.end()), ", ");
                    _print(ss.str());
                }
            }
            FTK_ASSERT(!readSystem->read(ftk::Path()));
            auto writeSystem = _context->getSystem<WriteSystem>();
            FTK_ASSERT(!writeSystem->write(ftk::Path(), IOInfo()));
        }

        void IOTest::_decode()
        {
            auto readSystem = _context->getSystem<ReadSystem>();
            auto writeSystem = _context->getSystem<WriteSystem>();
            const ftk::Size2I size(16, 16);

            // A decoder has to produce what the reader for the same format
            // produces; the reader is what everything still goes through.
            for (const std::string& ext : { ".exr", ".png", ".tif", ".tga" })
            {
                const ftk::Path path(
                    (_getTempDir() / ("IOTestDecode" + ext)).u8string());
                auto writePlugin = writeSystem->getPlugin(path);
                auto readPlugin = readSystem->getPlugin(path);
                if (!writePlugin || !readPlugin)
                {
                    continue;
                }
                auto decode = readPlugin->decode();
                if (!decode)
                {
                    _print(ftk::Format("{0}: no decoder").arg(ext));
                    continue;
                }

                const ftk::ImageInfo imageInfo = writePlugin->getInfo(
                    ftk::ImageInfo(size, ftk::ImageType::RGB_U8));
                if (ftk::ImageType::None == imageInfo.type)
                {
                    continue;
                }
                auto image = ftk::Image::create(imageInfo);
                image->zero();
                IOInfo info;
                info.video.push_back(imageInfo);
                const OTIO_NS::RationalTime time(0.0, 24.0);
                writeSystem->write(path, info)->writeVideo(time, image);

                // From the file, and from the same bytes in memory.
                auto fileIO = ftk::FileIO::create(path.get(), ftk::FileMode::Read);
                std::vector<uint8_t> bytes(fileIO->getSize());
                fileIO->read(bytes.data(), bytes.size());
                const ftk::MemFile memFile(nullptr, bytes.data(), bytes.size());

                const IOInfo decodeInfo = decode->getInfo(path.get());
                FTK_ASSERT(!decodeInfo.video.empty());
                FTK_ASSERT(decodeInfo.video[0].size == size);

                // One file says nothing about the sequence it is part of, so
                // the decoder leaves the time range alone.
                FTK_ASSERT(!isValid(decodeInfo.videoTime));

                const auto viaDecode = decode->readVideo(path.get(), nullptr, time);
                const auto viaMem = decode->readVideo(path.get(), &memFile, time);
                auto read = readPlugin->read(path);
                const auto viaRead = read->readVideo(time).get();

                FTK_ASSERT(viaDecode.image && viaMem.image && viaRead.image);
                const size_t byteCount = viaRead.image->getByteCount();
                FTK_ASSERT(viaDecode.image->getByteCount() == byteCount);
                FTK_ASSERT(viaMem.image->getByteCount() == byteCount);
                FTK_ASSERT(viaDecode.image->getType() == viaRead.image->getType());
                FTK_ASSERT(viaMem.image->getType() == viaRead.image->getType());
                FTK_ASSERT(0 == memcmp(
                    viaDecode.image->getData(), viaRead.image->getData(), byteCount));
                FTK_ASSERT(0 == memcmp(
                    viaMem.image->getData(), viaRead.image->getData(), byteCount));

                // The reader delegates to the decoder, so agreeing with it
                // proves only that the two are wired together. Compare against
                // what was written, which is what says the decode is right.
                FTK_ASSERT(byteCount == image->getByteCount());
                FTK_ASSERT(0 == memcmp(
                    viaDecode.image->getData(), image->getData(), byteCount));

                // A decoder holds no state, so it can be used from several
                // threads at once.
                std::atomic<bool> ok(true);
                std::vector<std::thread> threads;
                for (size_t i = 0; i < 8; ++i)
                {
                    threads.push_back(std::thread(
                        [decode, path, &memFile, time, &viaRead, byteCount, &ok]
                        {
                            for (size_t j = 0; j < 8; ++j)
                            {
                                const auto v = decode->readVideo(
                                    path.get(), &memFile, time);
                                if (!v.image || 0 != memcmp(
                                    v.image->getData(),
                                    viaRead.image->getData(),
                                    byteCount))
                                {
                                    ok = false;
                                }
                            }
                        }));
                }
                for (auto& thread : threads)
                {
                    thread.join();
                }
                FTK_ASSERT(ok);

                _print(ftk::Format("{0}: decoder matches reader").arg(ext));
            }
        }
    }
}

// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOTest/IOTest.h>

#include <tlRender/IO/SeqDecode.h>
#include <tlRender/IO/System.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/Image.h>
#include <ftk/Core/String.h>

#include <atomic>
#include <cstring>
#include <map>
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
            _cancellation();
            _videoData();
            _ioSystem();
            _decode();
            _seqDecode();
            _missingFrames();
            _seqRange();
            _structural();
        }

        namespace
        {
            class CancellationRead : public IRead
            {
            public:
                std::future<IOInfo> getInfo() override
                {
                    std::promise<IOInfo> promise;
                    promise.set_value(IOInfo());
                    return promise.get_future();
                }

                void cancelRequests() override
                {}
            };
        }

        void IOTest::_cancellation()
        {
            CancellationRead read;
            FTK_ASSERT(!read.isIOCancellationRequested());
            read.cancelIO();
            FTK_ASSERT(read.isIOCancellationRequested());

            // Cancellation is irreversible and safe to request repeatedly.
            read.cancelIO();
            FTK_ASSERT(read.isIOCancellationRequested());
        }

        void IOTest::_videoData()
        {
            {
                const VideoData v;
                FTK_CHECK(!v.image);
            }
            {
                const auto time = OTIO_NS::RationalTime(1.0, 24.0);
                const uint16_t layer = 1;
                const auto image = ftk::Image::create(160, 80, ftk::ImageType::L_U8);
                const VideoData v(time, layer, image);
                FTK_CHECK(time.strictly_equal(v.time));
                FTK_CHECK(layer == v.layer);
                FTK_CHECK(image == v.image);
            }
            {
                const auto time = OTIO_NS::RationalTime(1.0, 24.0);
                const uint16_t layer = 1;
                const auto image = ftk::Image::create(16, 16, ftk::ImageType::L_U8);
                const VideoData a(time, layer, image);
                VideoData b(time, layer, image);
                FTK_CHECK(a == b);
                b.time = OTIO_NS::RationalTime(2.0, 24.0);
                FTK_CHECK(a != b);
                FTK_CHECK(a < b);
            }
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
            FTK_CHECK(!readSystem->videoRead(ftk::Path()));
            FTK_CHECK(!readSystem->audioRead(ftk::Path()));
            auto writeSystem = _context->getSystem<WriteSystem>();
            FTK_CHECK(!writeSystem->write(ftk::Path(), IOInfo()));
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
                FTK_CHECK(!decodeInfo.video.empty());
                FTK_CHECK(decodeInfo.video[0].size == size);

                // One file says nothing about the sequence it is part of, so
                // the decoder leaves the time range alone.
                FTK_CHECK(!decodeInfo.videoTime.has_value());

                const auto viaDecode = decode->readVideo(path.get(), nullptr, time);
                const auto viaMem = decode->readVideo(path.get(), &memFile, time);

                // There is no reader to compare against any more; the image
                // that was written is the oracle, and the two decode paths
                // have to agree with it and with each other.
                FTK_CHECK(viaDecode.image && viaMem.image);
                const size_t byteCount = image->getByteCount();
                FTK_CHECK(viaDecode.image->getByteCount() == byteCount);
                FTK_CHECK(viaMem.image->getByteCount() == byteCount);
                FTK_CHECK(viaDecode.image->getType() == image->getType());
                FTK_CHECK(viaMem.image->getType() == image->getType());
                FTK_CHECK(0 == memcmp(
                    viaDecode.image->getData(), image->getData(), byteCount));
                FTK_CHECK(0 == memcmp(
                    viaMem.image->getData(), image->getData(), byteCount));

                // A decoder holds no state, so it can be used from several
                // threads at once.
                std::atomic<bool> ok(true);
                std::vector<std::thread> threads;
                for (size_t i = 0; i < 8; ++i)
                {
                    threads.push_back(std::thread(
                        [decode, path, &memFile, time, image, byteCount, &ok]
                        {
                            for (size_t j = 0; j < 8; ++j)
                            {
                                const auto v = decode->readVideo(
                                    path.get(), &memFile, time);
                                if (!v.image || 0 != memcmp(
                                    v.image->getData(),
                                    image->getData(),
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
                FTK_CHECK(ok);

                _print(ftk::Format("{0}: decoder matches what was written").arg(ext));
            }
        }
    
        void IOTest::_seqDecode()
        {
            // A sequence held in memory, which is how a bundle presents one:
            // the same frames the old reader walks, indexed off the path
            // number rather than found on disk.
            auto readSystem = _context->getSystem<ReadSystem>();
            auto writeSystem = _context->getSystem<WriteSystem>();
            const ftk::Size2I size(16, 16);
            const size_t frameCount = 5;
            // The sequence itself is never on disk: its frames are memory,
            // so only the one file the bytes come from is written.
            const ftk::Path path(
                (_getTempDir() / "IOTestSeq.0001.png").u8string());
            const ftk::Path srcPath(
                (_getTempDir() / "IOTestSeqSrc.png").u8string());
            auto writePlugin = writeSystem->getPlugin(path);
            auto readPlugin = readSystem->getPlugin(path);
            if (!writePlugin || !readPlugin)
            {
                return;
            }
            auto decode = readPlugin->decode();
            FTK_CHECK(decode);

            const ftk::ImageInfo imageInfo = writePlugin->getInfo(
                ftk::ImageInfo(size, ftk::ImageType::RGB_U8));
            IOInfo writeInfo;
            writeInfo.video.push_back(imageInfo);

            // Each frame is filled differently, so reading the wrong one
            // shows up rather than matching by accident.
            std::vector<std::shared_ptr<ftk::Image> > images;
            std::vector<std::vector<uint8_t> > bytes(frameCount);
            std::vector<ftk::MemFile> mem;
            for (size_t i = 0; i < frameCount; ++i)
            {
                auto image = ftk::Image::create(imageInfo);
                memset(image->getData(), static_cast<int>(17 + i * 40), image->getByteCount());
                images.push_back(image);
                // A trailing digit would be read as a frame number and every
                // frame would overwrite the same file.
                const std::string name =
                    std::string("IOTestSeqSrc") +
                    static_cast<char>('a' + i) + ".png";
                const ftk::Path framePath((_getTempDir() / name).u8string());
                writeSystem->write(framePath, writeInfo)->writeVideo(
                    OTIO_NS::RationalTime(0.0, 24.0), image);
                auto fileIO = ftk::FileIO::create(framePath.get(), ftk::FileMode::Read);
                bytes[i].resize(fileIO->getSize());
                fileIO->read(bytes[i].data(), bytes[i].size());
                mem.push_back(ftk::MemFile(nullptr, bytes[i].data(), bytes[i].size()));
            }
            auto image = images[0];

            IOOptions options;
            options["SeqIO/DefaultSpeed"] = "24";
            auto seq = SeqDecode::create(path, mem, decode, options);
            const IOInfo seqInfo = seq->getInfo();

            // The sequence knows the time range that no single file does.
            FTK_CHECK(seqInfo.videoTime.has_value());
            FTK_CHECK(seqInfo.videoTime->duration().value() == frameCount);
            FTK_CHECK(!seqInfo.video.empty());

            for (size_t i = 0; i < frameCount; ++i)
            {
                const OTIO_NS::RationalTime time(
                    static_cast<double>(1 + i), 24.0);
                const VideoData a = seq->readVideo(time);
                FTK_CHECK(a.image);
                FTK_CHECK(0 == memcmp(
                    a.image->getData(),
                    images[i]->getData(),
                    images[i]->getByteCount()));
            }

            // No thread of its own, so the caller decides where reads run.
            std::atomic<bool> ok(true);
            std::vector<std::thread> threads;
            for (size_t i = 0; i < 8; ++i)
            {
                threads.push_back(std::thread(
                    [frameCount, seq, &images, &ok]
                    {
                        for (size_t j = 0; j < frameCount; ++j)
                        {
                            const auto v = seq->readVideo(
                                OTIO_NS::RationalTime(
                                    static_cast<double>(1 + j), 24.0));
                            if (!v.image || 0 != memcmp(
                                v.image->getData(),
                                images[j]->getData(),
                                images[j]->getByteCount()))
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
            FTK_CHECK(ok);
            _print("sequence decode reads the right frame from memory");
        }

        void IOTest::_missingFrames()
        {
            // A sequence on disk with a gap in it, read under each of the
            // policies. Frames are found by reading them, so this covers the
            // path a render in progress actually takes.
            auto readSystem = _context->getSystem<ReadSystem>();
            auto writeSystem = _context->getSystem<WriteSystem>();
            const ftk::Size2I size(16, 16);
            const ftk::Path path(
                (_getTempDir() / "IOTestGap.0001.png").u8string());
            auto writePlugin = writeSystem->getPlugin(path);
            auto readPlugin = readSystem->getPlugin(path);
            if (!writePlugin || !readPlugin)
            {
                return;
            }
            auto decode = readPlugin->decode();
            FTK_CHECK(decode);

            const ftk::ImageInfo imageInfo = writePlugin->getInfo(
                ftk::ImageInfo(size, ftk::ImageType::RGB_U8));
            IOInfo writeInfo;
            writeInfo.video.push_back(imageInfo);

            // Frames 1, 2 and 5 exist; 3 and 4 do not. Each is filled
            // differently so that holding the wrong one shows up.
            const std::vector<int64_t> frames = { 1, 2, 5 };
            std::map<int64_t, std::shared_ptr<ftk::Image> > images;
            {
                // A sequence writer names each file from the time it is given,
                // so writing the gap is a matter of not asking for those two.
                auto write = writeSystem->write(path, writeInfo);
                for (int64_t frame : frames)
                {
                    auto image = ftk::Image::create(imageInfo);
                    memset(
                        image->getData(),
                        static_cast<int>(17 + frame * 40),
                        image->getByteCount());
                    images[frame] = image;
                    write->writeVideo(
                        OTIO_NS::RationalTime(static_cast<double>(frame), 24.0),
                        image);
                }
            }

            ftk::Path seqPath(path);
            seqPath.setFrames(ftk::RangeI64(1, 5));

            const auto readFrame = [&](MissingFrames policy, int64_t frame)
                {
                    IOOptions options;
                    options["SeqIO/DefaultSpeed"] = "24";
                    options["SeqIO/MissingFrames"] = to_string(policy);
                    auto seq = SeqDecode::create(seqPath, {}, decode, options);
                    return seq->readVideo(
                        OTIO_NS::RationalTime(static_cast<double>(frame), 24.0));
                };

            // A frame that is there reads the same under every policy.
            for (auto policy : getMissingFramesEnums())
            {
                const VideoData v = readFrame(policy, 2);
                FTK_CHECK(v.image);
                FTK_CHECK(0 == memcmp(
                    v.image->getData(),
                    images[2]->getData(),
                    images[2]->getByteCount()));
            }

            // Error: the read fails, as it did before there was a policy.
            {
                bool threw = false;
                try
                {
                    readFrame(MissingFrames::Error, 3);
                }
                catch (const std::exception&)
                {
                    threw = true;
                }
                FTK_CHECK(threw);
            }

            // Hold: both frames of the gap give frame 2, the last one before
            // it, and the time is still the time that was asked for.
            for (int64_t frame : { 3, 4 })
            {
                const VideoData v = readFrame(MissingFrames::Hold, frame);
                FTK_CHECK(v.image);
                FTK_CHECK(v.time.value() == frame);
                FTK_CHECK(0 == memcmp(
                    v.image->getData(),
                    images[2]->getData(),
                    images[2]->getByteCount()));

                // And it says so, so that a stand-in can be shown as one
                // rather than passing for the frame that was asked for.
                FTK_CHECK(v.missing);
                FTK_CHECK(v.heldFrom.has_value());
                FTK_CHECK(2 == v.heldFrom.value());
            }

            // A frame that is there says nothing of the sort.
            {
                const VideoData v = readFrame(MissingFrames::Hold, 2);
                FTK_CHECK(v.image);
                FTK_CHECK(!v.missing);
                FTK_CHECK(!v.heldFrom.has_value());
            }

            // Black: a blank frame of the right size and type.
            {
                const VideoData v = readFrame(MissingFrames::Black, 3);
                // Blank, so there is no frame it stands in for, but it is
                // still not the frame that was asked for.
                FTK_CHECK(v.missing);
                FTK_CHECK(!v.heldFrom.has_value());
                FTK_CHECK(v.image);
                FTK_CHECK(v.image->getSize() == size);
                std::vector<uint8_t> zero(v.image->getByteCount(), 0);
                FTK_CHECK(0 == memcmp(
                    v.image->getData(), zero.data(), zero.size()));
            }

            // A frame that is there but half written is missing as far as
            // this is concerned, which is what a frame still being rendered
            // looks like. Truncating 5 and 2 means holding from 5 has to walk
            // past two frames that are not there and one that will not read
            // before it reaches frame 1.
            for (int64_t frame : { 2, 5 })
            {
                const std::string fileName = seqPath.getFrame(frame, true);
                std::vector<uint8_t> bytes;
                {
                    auto fileIO = ftk::FileIO::create(
                        fileName, ftk::FileMode::Read);
                    bytes.resize(fileIO->getSize() / 2);
                    fileIO->read(bytes.data(), bytes.size());
                }
                auto fileIO = ftk::FileIO::create(
                    fileName, ftk::FileMode::Write);
                fileIO->write(bytes.data(), bytes.size());
            }
            {
                bool threw = false;
                try
                {
                    readFrame(MissingFrames::Error, 5);
                }
                catch (const std::exception&)
                {
                    threw = true;
                }
                FTK_CHECK(threw);

                const VideoData black = readFrame(MissingFrames::Black, 5);
                FTK_CHECK(black.image);

                const VideoData hold = readFrame(MissingFrames::Hold, 5);
                FTK_CHECK(hold.image);
                FTK_CHECK(0 == memcmp(
                    hold.image->getData(),
                    images[1]->getData(),
                    images[1]->getByteCount()));
            }

            _print("missing frames follow the policy");
        }

        void IOTest::_seqRange()
        {
            // A sequence opened over a range wider than the frames that
            // exist, which is how a render in progress is watched: the range
            // is the one the shot is meant to be, not the one reached so far.
            auto readSystem = _context->getSystem<ReadSystem>();
            auto writeSystem = _context->getSystem<WriteSystem>();
            const ftk::Size2I size(16, 16);
            const ftk::Path path(
                (_getTempDir() / "IOTestWide.0001.png").u8string());
            auto writePlugin = writeSystem->getPlugin(path);
            auto readPlugin = readSystem->getPlugin(path);
            if (!writePlugin || !readPlugin)
            {
                return;
            }
            auto decode = readPlugin->decode();
            FTK_CHECK(decode);

            IOInfo writeInfo;
            writeInfo.video.push_back(writePlugin->getInfo(
                ftk::ImageInfo(size, ftk::ImageType::RGB_U8)));

            // Only frames 50 and 51 have been rendered, so the frame the path
            // names is not there.
            {
                auto write = writeSystem->write(path, writeInfo);
                for (int64_t frame : { 50, 51 })
                {
                    write->writeVideo(
                        OTIO_NS::RationalTime(static_cast<double>(frame), 24.0),
                        ftk::Image::create(writeInfo.video[0]));
                }
            }

            ftk::Path seqPath(path);
            seqPath.setFrames(ftk::RangeI64(1, 60));

            IOOptions options;
            options["SeqIO/DefaultSpeed"] = "24";
            options["SeqIO/MissingFrames"] = to_string(MissingFrames::Black);
            auto seq = SeqDecode::create(seqPath, {}, decode, options);

            // Opening works, and the range is the one that was asked for
            // rather than the frames that happen to be there.
            const IOInfo seqInfo = seq->getInfo();
            FTK_CHECK(!seqInfo.video.empty());
            FTK_CHECK(seqInfo.video[0].size == size);
            FTK_CHECK(60 == seqInfo.videoTime->duration().value());
            FTK_CHECK(1 == seqInfo.videoTime->start_time().value());

            // A frame that has been rendered reads; one that has not follows
            // the policy.
            const VideoData rendered = seq->readVideo(
                OTIO_NS::RationalTime(50.0, 24.0));
            FTK_CHECK(rendered.image);
            const VideoData pending = seq->readVideo(
                OTIO_NS::RationalTime(10.0, 24.0));
            FTK_CHECK(pending.image);
            FTK_CHECK(pending.image->getSize() == size);

            _print("a sequence opens over a range wider than its frames");
        }

        void IOTest::_structural()
        {
            // A structural policy is not this layer's business: the timeline
            // built over a sequence decides which frames it covers, so here the
            // sequence keeps the range it was asked for and a frame number is a
            // frame number. What does change is that a frame which is not there
            // is a failure rather than something to fill in, because structure
            // promised it would not be asked for.
            auto readSystem = _context->getSystem<ReadSystem>();
            auto writeSystem = _context->getSystem<WriteSystem>();
            const ftk::Size2I size(16, 16);
            const ftk::Path path(
                (_getTempDir() / "IOTestStructural.0001.png").u8string());
            auto writePlugin = writeSystem->getPlugin(path);
            auto readPlugin = readSystem->getPlugin(path);
            if (!writePlugin || !readPlugin)
            {
                return;
            }
            auto decode = readPlugin->decode();
            FTK_CHECK(decode);

            IOInfo writeInfo;
            writeInfo.video.push_back(writePlugin->getInfo(
                ftk::ImageInfo(size, ftk::ImageType::RGB_U8)));

            // Every fifth frame, which is what a render in progress looks like.
            const std::vector<int64_t> frames = { 1, 6, 11, 16 };
            std::map<int64_t, std::shared_ptr<ftk::Image> > images;
            {
                auto write = writeSystem->write(path, writeInfo);
                for (int64_t frame : frames)
                {
                    auto image = ftk::Image::create(writeInfo.video[0]);
                    memset(
                        image->getData(),
                        static_cast<int>(17 + frame * 13),
                        image->getByteCount());
                    images[frame] = image;
                    write->writeVideo(
                        OTIO_NS::RationalTime(static_cast<double>(frame), 24.0),
                        image);
                }
            }

            ftk::Path seqPath(path);
            seqPath.setFrames(ftk::RangeI64(1, 20));
            for (auto missingFrames :
                { MissingFrames::Skip, MissingFrames::Gaps })
            {
                FTK_CHECK(isStructural(missingFrames));
                IOOptions options;
                options["SeqIO/DefaultSpeed"] = "24";
                options["SeqIO/MissingFrames"] = to_string(missingFrames);
                auto seq = SeqDecode::create(seqPath, {}, decode, options);

                // The whole range asked for, not the four frames that are
                // there: shortening it is the timeline's job.
                const IOInfo info = seq->getInfo();
                FTK_CHECK(20 == info.videoTime->duration().value());
                FTK_CHECK(1 == info.videoTime->start_time().value());

                // A frame that is there reads at its own number.
                for (int64_t frame : frames)
                {
                    const VideoData v = seq->readVideo(
                        OTIO_NS::RationalTime(
                            static_cast<double>(frame), 24.0));
                    FTK_CHECK(v.image);
                    FTK_CHECK(0 == memcmp(
                        v.image->getData(),
                        images[frame]->getData(),
                        images[frame]->getByteCount()));
                }

                // One that is not is an error rather than a held or blank
                // frame, since no clip should have asked for it.
                bool caught = false;
                try
                {
                    seq->readVideo(OTIO_NS::RationalTime(2.0, 24.0));
                }
                catch (const std::exception&)
                {
                    caught = true;
                }
                FTK_CHECK(caught);
            }

            _print("a structural policy leaves the sequence its own length");
        }
    }
}

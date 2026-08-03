// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOTest/EXRTest.h>

#include <tlRender/IO/EXR.h>
#include <tlRender/IO/System.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/FileIO.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/Image.h>

#include <cstring>
#include <sstream>

namespace tl
{
    namespace io_tests
    {
        EXRTest::EXRTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "io_tests::EXRTest")
        {}

        std::shared_ptr<EXRTest> EXRTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<EXRTest>(new EXRTest(context));
        }

        void EXRTest::run()
        {
            _partial();
            _enums();
            _util();
            _io();
        }

        void EXRTest::_enums()
        {
            using namespace exr;
            FTK_TEST_ENUM(Compression);
        }

        void EXRTest::_util()
        {
            {
                const std::set<std::string> data =
                {
                    "R",
                    ".G",
                    "B.",
                    "A",
                    "diffuse.R",
                    "diffuse.left.R"
                };
                const auto defaultChannels = exr::getDefaultChannels(data);
                const std::set<std::string> result = { ".G", "A", "B.", "R" };
                FTK_CHECK(defaultChannels == result);
            }
            {
                std::vector<std::string> data = { "A", "b", "g", "r" };
                exr::reorderChannels(data);
                const std::vector<std::string> result = { "r", "g", "b", "A" };
                FTK_CHECK(data == result);
            }
            {
                std::vector<std::string> data = { "z", "b", "G", "r" };
                exr::reorderChannels(data);
                const std::vector<std::string> result = { "r", "G", "b", "z" };
                FTK_CHECK(data == result);
            }
            {
                std::vector<std::string> data = { "diffuse.B", "diffuse.G", "diffuse.R" };
                exr::reorderChannels(data);
                const std::vector<std::string> result = { "diffuse.R", "diffuse.G", "diffuse.B" };
                FTK_CHECK(data == result);
            }
        }

        void EXRTest::write(
            const std::shared_ptr<IWritePlugin>& plugin,
            const std::shared_ptr<ftk::Image>& image,
            const ftk::Path& path,
            const ftk::ImageInfo& imageInfo,
            const ftk::ImageTags& tags,
            const IOOptions& options)
        {
            IOInfo info;
            info.video.push_back(imageInfo);
            info.videoTime = OTIO_NS::TimeRange(OTIO_NS::RationalTime(0.0, 24.0), OTIO_NS::RationalTime(1.0, 24.0));
            info.tags = tags;
            auto write = plugin->write(path, info, options);
            write->writeVideo(OTIO_NS::RationalTime(0.0, 24.0), image);
        }

        void EXRTest::read(
            const std::shared_ptr<IReadPlugin>& plugin,
            const std::shared_ptr<ftk::Image>& image,
            const ftk::Path& path,
            bool memoryIO,
            const ftk::ImageTags& tags,
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
            const auto frameTags = videoData.image->getTags();
            for (const auto& j : frameTags)
            {
                if (const auto k = tags.find(j.first); k != tags.end())
                {
                    FTK_CHECK(k->second == j.second);
                }
            }
        }

        void EXRTest::readError(
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

        void EXRTest::_io()
        {
            auto readSystem = _context->getSystem<ReadSystem>();
            auto readPlugin = readSystem->getPlugin<exr::ReadPlugin>();
            auto writeSystem = _context->getSystem<WriteSystem>();
            auto writePlugin = writeSystem->getPlugin<exr::WritePlugin>();

            const ftk::ImageTags tags =
            {
                //{ "Name", "Name" },
                //{ "Type", "scanlineimage" },
                //{ "Version", "1" },
                //{ "Chunk Count", "1" },
                //{ "View", "View" },
                //{ "Tile", "1 2 1 1" },
                { "AdoptedNeutral", "0 1" },
                { "Altitude", "1" },
                { "Aperture", "1" },
                { "AscFramingDecisionList", "AscFramingDecisionList" },
                { "CameraCCTSetting", "1" },
                { "CameraColorBalance", "1 2" },
                { "CameraFirmwareVersion", "CameraFirmwareVersion" },
                { "CameraLabel", "CameraLabel" },
                { "CameraMake", "CameraMake" },
                { "CameraModel", "CameraModel" },
                { "CameraSerialNumber", "CameraSerialNumber" },
                { "CameraTintSetting", "1" },
                { "CameraTintSetting", "CameraTintSetting" },
                { "CapDate", "CapDate" },
                { "CaptureRate", "24 1" },
                { "Chromaticities", "0 1 2 3 4 5 6 7" },
                { "Comments", "Comments" },
                { "EffectiveFocalLength", "1" },
                { "EntrancePupilOffset", "1" },
                { "Envmap", "1" },
                { "ExpTime", "1" },
                { "Focus", "1" },
                { "FramesPerSecond", "24 1" },
                { "ImageCounter", "1" },
                { "IsoSpeed", "1" },
                { "KeyCode", "1:2:3:4:5:6:20" },
                { "Latitude", "1" },
                { "LensFirmwareVersion", "LensFirmwareVersion" },
                { "LensMake", "LensMake" },
                { "LensModel", "LensModel" },
                { "LensSerialNumber", "LensSerialNumber" },
                { "Longitude", "1" },
                { "NominalFocalLength", "1" },
                { "OriginalDataWindow", "0 1 2 3" },
                { "Owner", "Owner" },
                { "PinholeFocalLength", "1" },
                { "ReelName", "ReelName" },
                { "SensorAcquisitionRectangle", "0 1 2 3" },
                { "SensorCenterOffset", "0 1" },
                { "SensorPhotositePitch", "1" },
                { "ShutterAngle", "1" },
                { "TStop", "1" },
                { "TimeCode", "01:00:00:00" },
                { "UtcOffset", "1" },
                { "WhiteLuminance", "1" },
                { "WorldToCamera", "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15" },
                { "WorldToNDC", "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15" },
                { "XDensity", "1" },

                { "Wrapmodes", "Wrapmodes" },
                { "MultiView", "5:hello0:5:world" },
                { "DeepImageState", "1" },

                /*{ "X Density", "1" },
                { "Owner", "Owner" },
                { "Comments", "Comments" },
                { "Capture Date", "Capture Date" },
                { "UTC Offset", "1" },
                { "Longitude", "1" },
                { "Latitude", "1" },
                { "Altitude", "1" },
                { "Focus", "1" },
                { "Exposure Time", "1" },
                { "Aperture", "1" },
                { "ISO Speed", "1" },
                { "Environment Map", "1" },
                { "Keycode", "1 2 3 4 5 6 7" },
                { "Timecode", "01:02:03:04" },
                { "Wrap Modes", "Wrap Modes" }*/
            };
            const std::vector<std::string> fileNames =
            {
                "EXRTest",
                "大平原"
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
            const std::vector<std::pair<std::string, std::string> > options =
            {
                { "OpenEXR/ChannelGrouping", "None" },
                { "OpenEXR/ChannelGrouping", "Known" },
                { "OpenEXR/ChannelGrouping", "All" },
                { "OpenEXR/Compression", "None" },
                { "OpenEXR/Compression", "RLE" },
                { "OpenEXR/Compression", "ZIPS" },
                { "OpenEXR/Compression", "ZIP" },
                { "OpenEXR/Compression", "PIZ" },
                { "OpenEXR/Compression", "PXR24" },
                { "OpenEXR/Compression", "B44" },
                { "OpenEXR/Compression", "B44A" },
                { "OpenEXR/Compression", "DWAA" },
                { "OpenEXR/Compression", "DWAB" },
                { "OpenEXR/DWACompressionLevel", "45" },
                { "OpenEXR/DWACompressionLevel", "100" }
            };

            // These dimensions do not interact: a compression setting
            // behaves the same on a non-ASCII path, and a pixel type behaves
            // the same under either channel grouping. Crossing all of them
            // ran thousands of round trips to cover what sweeping each one
            // covers, and it dominated how long the suite took.
            struct Case
            {
                std::string fileName;
                bool memoryIO = false;
                ftk::Size2I size;
                ftk::ImageType pixelType = ftk::ImageType::RGBA_F16;
                std::pair<std::string, std::string> option;
            };
            Case base;
            base.fileName = fileNames[0];
            base.size = ftk::Size2I(16, 16);
            base.option = { "OpenEXR/Compression", "ZIP" };

            std::vector<Case> cases;
            // Every pixel type at every size, from a file and from memory.
            for (const bool memoryIO : memoryIOList)
            {
                for (const auto& size : sizes)
                {
                    for (const auto pixelType : ftk::getImageTypeEnums())
                    {
                        Case c = base;
                        c.memoryIO = memoryIO;
                        c.size = size;
                        c.pixelType = pixelType;
                        cases.push_back(c);
                    }
                }
            }
            // Every option, from a file and from memory.
            for (const bool memoryIO : memoryIOList)
            {
                for (const auto& option : options)
                {
                    Case c = base;
                    c.memoryIO = memoryIO;
                    c.option = option;
                    cases.push_back(c);
                }
            }
            // Every path.
            for (const auto& fileName : fileNames)
            {
                Case c = base;
                c.fileName = fileName;
                cases.push_back(c);
            }

            for (const auto& c : cases)
            {
                IOOptions ioOptions;
                ioOptions[c.option.first] = c.option.second;
                const auto imageInfo = writePlugin->getInfo(
                    ftk::ImageInfo(c.size, c.pixelType));
                if (imageInfo.isValid())
                {
                    ftk::Path path;
                    {
                        std::stringstream ss;
                        ss << c.fileName << ' ' << c.size << ' ' <<
                            c.pixelType << ".0.exr";
                        _print(ss.str());
                        path = ftk::Path((_getTempDir() / ss.str()).u8string());
                    }
                    auto image = ftk::Image::create(imageInfo);
                    image->zero();
                    image->setTags(tags);
                    try
                    {
                        write(writePlugin, image, path, imageInfo, tags, ioOptions);
                        read(readPlugin, image, path, c.memoryIO, tags, ioOptions);
                        readError(readPlugin, image, path, c.memoryIO, ioOptions);
                    }
                    catch (const std::exception& e)
                    {
                        _error(e.what());
                    }
                }
            }
        }

        void EXRTest::_partial()
        {
            // An EXR still being written. The header is there and so are the
            // first scanlines, and DJV 1.x showed those rather than nothing;
            // losing the whole frame is what issue #308 is about.
            auto readSystem = _context->getSystem<ReadSystem>();
            auto writeSystem = _context->getSystem<WriteSystem>();
            const ftk::Path path((_getTempDir() / "EXRPartial.exr").u8string());
            auto writePlugin = writeSystem->getPlugin(path);
            auto readPlugin = readSystem->getPlugin(path);
            if (!writePlugin || !readPlugin)
            {
                return;
            }
            auto decode = readPlugin->decode();
            if (!decode)
            {
                return;
            }

            // Written with no compression, so that cutting the file leaves
            // whole scanlines behind rather than half of a compressed block.
            const ftk::Size2I size(64, 64);
            const ftk::ImageInfo imageInfo = writePlugin->getInfo(
                ftk::ImageInfo(size, ftk::ImageType::RGB_F16));
            IOInfo writeInfo;
            writeInfo.video.push_back(imageInfo);
            auto image = ftk::Image::create(imageInfo);
            std::memset(image->getData(), 0xff, image->getByteCount());
            IOOptions writeOptions;
            writeOptions["OpenEXR/Compression"] = "None";
            writeSystem->write(path, writeInfo, writeOptions)->writeVideo(
                OTIO_NS::RationalTime(0.0, 24.0), image);

            std::vector<uint8_t> whole;
            {
                auto fileIO = ftk::FileIO::create(path.get(), ftk::FileMode::Read);
                whole.resize(fileIO->getSize());
                fileIO->read(whole.data(), whole.size());
            }

            // Two thirds of the file: the header and most of the scanlines.
            const ftk::Path partialPath(
                (_getTempDir() / "EXRPartialCut.exr").u8string());
            {
                auto fileIO = ftk::FileIO::create(partialPath.get(), ftk::FileMode::Write);
                fileIO->write(whole.data(), whole.size() * 2 / 3);
            }
            try
            {
                const VideoData v = decode->readVideo(
                    partialPath.get(), nullptr, OTIO_NS::RationalTime(0.0, 24.0));
                FTK_CHECK(v.image);
                FTK_CHECK(v.image->getSize() == size);

                // Some of it was read, and some of it was not.
                const uint8_t* data = v.image->getData();
                const size_t byteCount = v.image->getByteCount();
                size_t set = 0;
                for (size_t i = 0; i < byteCount; ++i)
                {
                    if (data[i] != 0)
                    {
                        ++set;
                    }
                }
                _print(ftk::Format("Partial EXR: {0} of {1} bytes read").
                    arg(set).arg(byteCount));
                FTK_CHECK(set > 0);
                FTK_CHECK(set < byteCount);
            }
            catch (const std::exception& e)
            {
                _error(ftk::Format("A partial EXR should still read: {0}").
                    arg(e.what()));
            }

            // Only the header, so there is not one scanline to show. That is
            // an unreadable file rather than one part way through, and it has
            // to say so instead of handing back a blank frame.
            const ftk::Path headerPath(
                (_getTempDir() / "EXRHeaderOnly.exr").u8string());
            {
                auto fileIO = ftk::FileIO::create(headerPath.get(), ftk::FileMode::Write);
                fileIO->write(whole.data(), std::min<size_t>(whole.size(), 200));
            }
            {
                bool threw = false;
                try
                {
                    decode->readVideo(
                        headerPath.get(), nullptr, OTIO_NS::RationalTime(0.0, 24.0));
                }
                catch (const std::exception&)
                {
                    threw = true;
                }
                FTK_CHECK(threw);
            }
        }
    }
}

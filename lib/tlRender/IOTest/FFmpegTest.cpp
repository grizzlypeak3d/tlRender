// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOTest/FFmpegTest.h>

#include <tlRender/IO/FFmpeg.h>
#include <tlRender/IO/System.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/FileIO.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <future>
#include <sstream>

namespace tl
{
    namespace io_tests
    {
        FFmpegTest::FFmpegTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "io_tests::FFmpegTest")
        {}

        std::shared_ptr<FFmpegTest> FFmpegTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<FFmpegTest>(new FFmpegTest(context));
        }

        void FFmpegTest::run()
        {
            _convert();
            _io();
            _audio();
            _split();
        }

        namespace
        {
            void write(
                const std::shared_ptr<IWritePlugin>& plugin,
                const std::shared_ptr<ftk::Image>& image,
                const ftk::Path& path,
                const ftk::ImageInfo& imageInfo,
                const ftk::ImageTags& tags,
                const OTIO_NS::RationalTime& duration,
                const IOOptions& options)
            {
                IOInfo info;
                info.video.push_back(imageInfo);
                info.videoTime = OTIO_NS::TimeRange(OTIO_NS::RationalTime(0.0, 24.0), duration);
                info.tags = tags;
                auto write = plugin->write(path, info, options);
                for (size_t i = 0; i < static_cast<size_t>(duration.value()); ++i)
                {
                    write->writeVideo(OTIO_NS::RationalTime(i, 24.0), image);
                }
                write->finish();
            }

            void read(
                const std::shared_ptr<IReadPlugin>& plugin,
                const std::shared_ptr<ftk::Image>& image,
                const ftk::Path& path,
                bool memoryIO,
                const ftk::ImageTags& tags,
                const OTIO_NS::RationalTime& duration,
                const IOOptions& options)
            {
                std::vector<uint8_t> memoryData;
                std::vector<ftk::MemFile> memory;
                std::shared_ptr<IVideoRead> read;
                if (memoryIO)
                {
                    auto fileIO = ftk::FileIO::create(path.get(), ftk::FileMode::Read);
                    memoryData.resize(fileIO->getSize());
                    fileIO->read(memoryData.data(), memoryData.size());
                    memory.push_back(ftk::MemFile(nullptr, memoryData.data(), memoryData.size()));
                    read = plugin->videoRead(path, memory, options);
                }
                else
                {
                    read = plugin->videoRead(path, options);
                }
                const auto ioInfo = read->getInfo().get();
                FTK_ASSERT(!ioInfo.video.empty());
                for (size_t i = 0; i < static_cast<size_t>(duration.value()); ++i)
                {
                    const auto videoData = read->readVideo(OTIO_NS::RationalTime(i, 24.0)).get();
                    FTK_ASSERT(videoData.image);
                    FTK_ASSERT(videoData.image->getSize() == image->getSize());
                    const auto frameTags = videoData.image->getTags();
                    for (const auto& j : tags)
                    {
                        const auto k = frameTags.find(j.first);
                        FTK_ASSERT(k != frameTags.end());
                        FTK_ASSERT(k->second == j.second);
                    }
                }
                for (size_t i = 0; i < static_cast<size_t>(duration.value()); ++i)
                {
                    const auto videoData = read->readVideo(OTIO_NS::RationalTime(i, 24.0)).get();
                }
            }

            void readError(
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
                auto read = plugin->videoRead(path, memory, options);
                // This previously hung on truncated files (suspected cause:
                // the avIOBufferSeek() whence/return bug, since fixed). Use
                // a watchdog rather than a bare get() so a regression fails
                // the test instead of hanging the suite. Any result --
                // including empty -- is acceptable; completing is what
                // matters.
                auto future = read->readVideo(OTIO_NS::RationalTime(0.0, 24.0));
                if (future.wait_for(std::chrono::seconds(30)) !=
                    std::future_status::ready)
                {
                    throw std::runtime_error(
                        "Timeout reading a truncated file");
                }
                const auto videoData = future.get();
                // The truncated file must record a queryable error;
                // silently returning empty data is not acceptable.
                if (read->getError().empty() || 0 == read->getErrorCount())
                {
                    throw std::runtime_error(
                        "Expected an error reading a truncated file");
                }
            }
        }

        void FFmpegTest::_io()
        {
            auto readSystem = _context->getSystem<ReadSystem>();
            auto readPlugin = readSystem->getPlugin<ffmpeg::ReadPlugin>();
            auto writeSystem = _context->getSystem<WriteSystem>();
            auto writePlugin = writeSystem->getPlugin<ffmpeg::WritePlugin>();

            const ftk::ImageTags tags =
            {
                { "artist", "artist" },
                { "comment", "comment" },
                { "title", "title" }
            };
            const std::vector<std::string> fileNames =
            {
                "FFmpegTest",
                "大平原"
            };
            const std::vector<bool> memoryIOList =
            {
                false,
                true
            };
            const std::vector<ftk::Size2I> sizes =
            {
                // Large enough to encode as more than one macroblock, which
                // is what distinguishes this from the small size below.
                // Every pixel here is encoded and decoded again for each
                // pixel type and option, so the resolution sets how long
                // this test takes.
                ftk::Size2I(160, 120),
                ftk::Size2I(80, 60),
                ftk::Size2I(0, 0)
            };
            const std::vector<std::pair<std::string, std::string> > options =
            {
                { "FFmpeg/YUVToRGB", "1" },
                { "FFmpeg/ThreadCount", "1" },
                { "FFmpeg/RequestTimeout", "1" },
                { "FFmpeg/VideoBufferSize", "1" },
                { "FFmpeg/AudioBufferSize", "1/1" },
                { "FFmpeg/Codec", "mjpeg" },
                { "FFmpeg/Codec", "v210" },
                { "FFmpeg/Codec", "v410" }
            };

            for (const auto& fileName : fileNames)
            {
                for (const bool memoryIO : memoryIOList)
                {
                    for (const auto& size : sizes)
                    {
                        for (const auto pixelType : ftk::getImageTypeEnums())
                        {
                            for (const auto& option : options)
                            {
                                IOOptions options;
                                // The writer intentionally has no default
                                // video codec, so every row needs one;
                                // rows that test a codec override it.
                                options["FFmpeg/Codec"] = "mjpeg";
                                options[option.first] = option.second;
                                const std::string codec = options["FFmpeg/Codec"];
                                // MP4 cannot tag the uncompressed v210 and
                                // v410 codecs; they require MOV.
                                const std::string extension =
                                    ("v210" == codec || "v410" == codec) ?
                                    ".mov" :
                                    ".mp4";
                                const auto imageInfo = writePlugin->getInfo(ftk::ImageInfo(size, pixelType));
                                if (imageInfo.isValid())
                                {
                                    ftk::Path path;
                                    {
                                        std::stringstream ss;
                                        ss << fileName << ' ' << size << ' ' <<
                                            pixelType << ' ' << codec << extension;
                                        _print(ss.str());
                                        path = ftk::Path((_getTempDir() / ss.str()).u8string());
                                    }
                                    auto image = ftk::Image::create(imageInfo);
                                    image->zero();
                                    image->setTags(tags);
                                    const OTIO_NS::RationalTime duration(24.0, 24.0);
                                    try
                                    {
                                        write(writePlugin, image, path, imageInfo, tags, duration, options);
                                        read(readPlugin, image, path, memoryIO, tags, duration, options);
                                        readError(readPlugin, image, path, memoryIO, options);
                                    }
                                    catch (const std::exception& e)
                                    {
                                        _error(e.what());
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        void FFmpegTest::_convert()
        {
            // Round-trip a known color image through the writer and back through
            // the reader with YUV->RGB conversion forced ON, then check pixels.
            //
            // The pre-existing _io() test writes a *zeroed* (black) image and
            // only asserts size and tags -- black survives every color bug, so
            // it cannot catch a broken conversion. This test paints vertical
            // color stripes (invariant under the reader's y-mirror) and verifies
            // each stripe's center survives the round-trip. The tolerance is
            // deliberately loose: it sits far above codec + colorspace rounding
            // on a flat stripe interior, but far below a gross failure such as
            // black output or a saturated channel. It is a regression guard for
            // the conversion path, not a colorimetric assertion.
            auto readSystem = _context->getSystem<ReadSystem>();
            auto readPlugin = readSystem->getPlugin<ffmpeg::ReadPlugin>();
            auto writeSystem = _context->getSystem<WriteSystem>();
            auto writePlugin = writeSystem->getPlugin<ffmpeg::WritePlugin>();

            // Expected stripe colors, full-range RGB (16-bit). Primaries near
            // saturation plus near-black and near-white exercise the matrix and
            // the limited/full range handling.
            const std::array<std::array<uint16_t, 3>, 6> stripes =
            {{
                {{ 60000,  2000,  2000 }},  // red
                {{  2000, 60000,  2000 }},  // green
                {{  2000,  2000, 60000 }},  // blue
                {{ 32768, 32768, 32768 }},  // mid gray
                {{  4000,  4000,  4000 }},  // near black
                {{ 61000, 61000, 61000 }}   // near white
            }};

            const ftk::Size2I size(360, 240);
            const int stripeW = size.w / static_cast<int>(stripes.size());

            const auto imageInfo = ftk::ImageInfo(size, ftk::ImageType::RGB_U16);
            if (!writePlugin->getInfo(imageInfo).isValid())
            {
                _print("_convert: writer does not support RGB_U16; skipping");
                return;
            }
            auto image = ftk::Image::create(imageInfo);
            {
                uint16_t* p = reinterpret_cast<uint16_t*>(image->getData());
                for (int y = 0; y < size.h; ++y)
                {
                    for (int x = 0; x < size.w; ++x)
                    {
                        const size_t s = std::min<size_t>(
                            x / stripeW, stripes.size() - 1);
                        *p++ = stripes[s][0];
                        *p++ = stripes[s][1];
                        *p++ = stripes[s][2];
                    }
                }
            }

            // Sample a pixel as normalized [0,1] RGB, tolerating whatever bit
            // depth the conversion produced (8- or 16-bit RGB).
            auto sample = [](
                const std::shared_ptr<ftk::Image>& img,
                int x, int y) -> std::array<float, 3>
            {
                const int w = img->getSize().w;
                const uint8_t* const d = img->getData();
                std::array<float, 3> out = {{ 0.f, 0.f, 0.f }};
                switch (img->getInfo().type)
                {
                case ftk::ImageType::RGB_U16:
                {
                    const uint16_t* const px =
                        reinterpret_cast<const uint16_t*>(d) +
                        (static_cast<size_t>(y) * w + x) * 3;
                    for (int c = 0; c < 3; ++c) out[c] = px[c] / 65535.f;
                    break;
                }
                case ftk::ImageType::RGB_U8:
                {
                    const uint8_t* const px =
                        d + (static_cast<size_t>(y) * w + x) * 3;
                    for (int c = 0; c < 3; ++c) out[c] = px[c] / 255.f;
                    break;
                }
                default: break;
                }
                return out;
            };

            const ftk::Path path((_getTempDir() / "FFmpegConvertTest.mov").u8string());
            try
            {
                IOInfo info;
                info.video.push_back(imageInfo);
                info.videoTime = OTIO_NS::TimeRange(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(1.0, 24.0));
                {
                    // The writer has no default video codec, so one must
                    // be given or the write throws and this test silently
                    // skips its pixel checks.
                    IOOptions writeOptions;
                    writeOptions["FFmpeg/Codec"] = "mjpeg";
                    auto write = writePlugin->write(path, info, writeOptions);
                    write->writeVideo(OTIO_NS::RationalTime(0.0, 24.0), image);
                    write->finish();
                }

                IOOptions readOptions;
                readOptions["FFmpeg/YUVToRGB"] = "1";
                auto read = readPlugin->videoRead(path, readOptions);
                const auto ioInfo = read->getInfo().get();
                FTK_ASSERT(!ioInfo.video.empty());
                const auto videoData =
                    read->readVideo(OTIO_NS::RationalTime(0.0, 24.0)).get();
                FTK_ASSERT(videoData.image);

                const ftk::ImageType type = videoData.image->getInfo().type;
                if (type != ftk::ImageType::RGB_U16 &&
                    type != ftk::ImageType::RGB_U8)
                {
                    _print("_convert: unexpected output type; skipping pixel check");
                    return;
                }

                const int yMid = videoData.image->getSize().h / 2;
                const float tolerance = 0.12f; // ~8000/65535
                for (size_t i = 0; i < stripes.size(); ++i)
                {
                    const int xMid =
                        static_cast<int>(i) * stripeW + stripeW / 2;
                    const auto got = sample(videoData.image, xMid, yMid);
                    const std::array<float, 3> want =
                    {{
                        stripes[i][0] / 65535.f,
                        stripes[i][1] / 65535.f,
                        stripes[i][2] / 65535.f
                    }};
                    for (int c = 0; c < 3; ++c)
                    {
                        if (std::fabs(got[c] - want[c]) > tolerance)
                        {
                            std::stringstream ss;
                            ss << "Color conversion mismatch, stripe " << i <<
                                " channel " << c << ": expected " << want[c] <<
                                " got " << got[c];
                            _error(ss.str());
                        }
                        FTK_ASSERT(std::fabs(got[c] - want[c]) <= tolerance);
                    }
                }
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }
        }

        namespace
        {
            // Deterministic 440Hz sine, amplitude .5, S16 interleaved. The
            // sample offset keeps the phase continuous across chunks so the
            // written signal is identical no matter how it is chunked.
            std::shared_ptr<Audio> makeSine(
                const AudioInfo& info,
                size_t sampleCount,
                size_t sampleOffset)
            {
                auto out = Audio::create(info, sampleCount);
                int16_t* data = reinterpret_cast<int16_t*>(out->getData());
                const double pi = 3.14159265358979323846;
                for (size_t i = 0; i < sampleCount; ++i)
                {
                    const double t = (sampleOffset + i) /
                        static_cast<double>(info.sampleRate);
                    const int16_t v = static_cast<int16_t>(
                        std::sin(2.0 * pi * 440.0 * t) * .5 * 32767.0);
                    for (int c = 0; c < info.channelCount; ++c)
                    {
                        *data++ = v;
                    }
                }
                return out;
            }
        }

        void FFmpegTest::_audio()
        {
            // Exercise the audio write path: a lossless PCM round trip that
            // must be sample-accurate, a lossy round trip with the container
            // default codec (checking structure and signal energy), error
            // handling for a bad codec name, and graceful skipping when the
            // container cannot hold audio.
            auto readSystem = _context->getSystem<ReadSystem>();
            auto readPlugin = readSystem->getPlugin<ffmpeg::ReadPlugin>();
            auto writeSystem = _context->getSystem<WriteSystem>();
            auto writePlugin = writeSystem->getPlugin<ffmpeg::WritePlugin>();

            const AudioInfo audioInfo(2, AudioType::S16, 44100);
            const size_t sampleCount = 66150; // 1.5 seconds
            const size_t frameCount = 36;     // 1.5 seconds at 24 FPS
            const auto imageInfo = writePlugin->getInfo(
                ftk::ImageInfo(ftk::Size2I(80, 60), ftk::ImageType::RGB_U8));
            auto image = ftk::Image::create(imageInfo);
            image->zero();

            // Write a movie with video and audio, interleaving one second
            // audio chunks with the video frames like BakeApp. The final
            // chunk is a half second to exercise trimming and the encoder
            // FIFO flush.
            auto writeMovie = [&](const ftk::Path& path, const IOOptions& options)
            {
                IOInfo info;
                info.video.push_back(imageInfo);
                info.videoTime = OTIO_NS::TimeRange(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(
                        static_cast<double>(frameCount), 24.0));
                info.audio = audioInfo;
                info.audioTime = OTIO_NS::TimeRange(
                    OTIO_NS::RationalTime(0.0, audioInfo.sampleRate),
                    OTIO_NS::RationalTime(
                        static_cast<double>(sampleCount),
                        audioInfo.sampleRate));
                auto write = writePlugin->write(path, info, options);
                size_t samplesWritten = 0;
                for (size_t frame = 0; frame < frameCount; ++frame)
                {
                    write->writeVideo(
                        OTIO_NS::RationalTime(
                            static_cast<double>(frame), 24.0),
                        image);
                    const double videoSeconds = (frame + 1) / 24.0;
                    while (samplesWritten < sampleCount &&
                        samplesWritten < static_cast<size_t>(
                            videoSeconds * audioInfo.sampleRate))
                    {
                        const size_t chunk = std::min<size_t>(
                            audioInfo.sampleRate,
                            sampleCount - samplesWritten);
                        write->writeAudio(
                            OTIO_NS::TimeRange(
                                OTIO_NS::RationalTime(
                                    static_cast<double>(samplesWritten),
                                    audioInfo.sampleRate),
                                OTIO_NS::RationalTime(
                                    static_cast<double>(chunk),
                                    audioInfo.sampleRate)),
                            makeSine(audioInfo, chunk, samplesWritten));
                        samplesWritten += chunk;
                    }
                }
                write->finish();
                // finish() is idempotent; a second call is a no-op.
                write->finish();
            };

            // Lossless round trip: PCM audio must survive sample-exactly,
            // proving the resampler, FIFO chunking, flush, and pts handling
            // do not drop, duplicate, or reorder samples.
            try
            {
                _print("_audio: PCM round trip");
                const ftk::Path path(
                    (_getTempDir() / "FFmpegAudioPCM.mov").u8string());
                IOOptions options;
                options["FFmpeg/Codec"] = "mjpeg";
                options["FFmpeg/AudioCodec"] = "pcm_s16le";
                writeMovie(path, options);

                auto read = readPlugin->audioRead(path);
                const auto ioInfo = read->getInfo().get();
                if (!ioInfo.audio.isValid() ||
                    ioInfo.audio.channelCount != audioInfo.channelCount ||
                    ioInfo.audio.type != AudioType::S16 ||
                    ioInfo.audio.sampleRate != audioInfo.sampleRate)
                {
                    _error("PCM: unexpected audio info");
                    FTK_ASSERT(false);
                }
                const int64_t readSampleCount = static_cast<int64_t>(
                    ioInfo.audioTime.duration().
                        rescaled_to(audioInfo.sampleRate).value());
                if (std::abs(
                    readSampleCount - static_cast<int64_t>(sampleCount)) > 64)
                {
                    std::stringstream ss;
                    ss << "PCM: unexpected sample count: " << readSampleCount <<
                        ", expected: " << sampleCount;
                    _error(ss.str());
                    FTK_ASSERT(false);
                }

                size_t mismatches = 0;
                size_t offset = 0;
                const size_t compareCount = std::min(
                    sampleCount,
                    static_cast<size_t>(std::max(
                        static_cast<int64_t>(0), readSampleCount)));
                while (offset < compareCount)
                {
                    const size_t chunk = std::min<size_t>(
                        audioInfo.sampleRate, compareCount - offset);
                    const auto audioData = read->readAudio(
                        OTIO_NS::TimeRange(
                            ioInfo.audioTime.start_time() +
                                OTIO_NS::RationalTime(
                                    static_cast<double>(offset),
                                    audioInfo.sampleRate),
                            OTIO_NS::RationalTime(
                                static_cast<double>(chunk),
                                audioInfo.sampleRate))).get();
                    if (!audioData.audio ||
                        audioData.audio->getSampleCount() != chunk ||
                        audioData.audio->getType() != AudioType::S16)
                    {
                        _error("PCM: unexpected audio data");
                        FTK_ASSERT(false);
                        break;
                    }
                    const auto expected = makeSine(audioInfo, chunk, offset);
                    const int16_t* a = reinterpret_cast<const int16_t*>(
                        audioData.audio->getData());
                    const int16_t* b = reinterpret_cast<const int16_t*>(
                        expected->getData());
                    for (size_t i = 0;
                        i < chunk * audioInfo.channelCount;
                        ++i)
                    {
                        if (std::abs(a[i] - b[i]) > 1)
                        {
                            ++mismatches;
                        }
                    }
                    offset += chunk;
                }
                if (mismatches > 0)
                {
                    std::stringstream ss;
                    ss << "PCM: " << mismatches << " samples do not match";
                    _error(ss.str());
                    FTK_ASSERT(false);
                }

                // A request expressed at a different rate than the sample
                // rate (here: seconds) must still be allocated and copied
                // in samples.
                const auto audioData = read->readAudio(
                    OTIO_NS::TimeRange(
                        OTIO_NS::RationalTime(0.0, 1.0),
                        OTIO_NS::RationalTime(1.0, 1.0))).get();
                if (!audioData.audio ||
                    audioData.audio->getSampleCount() !=
                        static_cast<size_t>(audioInfo.sampleRate))
                {
                    std::stringstream ss;
                    ss << "PCM: seconds-based request returned " <<
                        (audioData.audio ?
                            audioData.audio->getSampleCount() : 0) <<
                        " samples, expected " << audioInfo.sampleRate;
                    _error(ss.str());
                    FTK_ASSERT(false);
                }
                else
                {
                    const auto expected = makeSine(audioInfo, 1000, 0);
                    const int16_t* a = reinterpret_cast<const int16_t*>(
                        audioData.audio->getData());
                    const int16_t* b = reinterpret_cast<const int16_t*>(
                        expected->getData());
                    size_t secondsMismatches = 0;
                    for (size_t i = 0;
                        i < 1000 * audioInfo.channelCount;
                        ++i)
                    {
                        if (std::abs(a[i] - b[i]) > 1)
                        {
                            ++secondsMismatches;
                        }
                    }
                    if (secondsMismatches > 0)
                    {
                        std::stringstream ss;
                        ss << "PCM: seconds-based request: " <<
                            secondsMismatches << " samples do not match";
                        _error(ss.str());
                        FTK_ASSERT(false);
                    }
                }

                // A clean file must not record any errors.
                if (!read->getError().empty() || read->getErrorCount() > 0)
                {
                    std::stringstream ss;
                    ss << "PCM: unexpected read errors: " <<
                        read->getErrorCount() << ": " << read->getError();
                    _error(ss.str());
                    FTK_ASSERT(false);
                }
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }

            // Lossy round trip with the container default codec (AAC in
            // MOV): the exact samples will not survive, so check the
            // structure and that the signal energy is preserved. The RMS
            // window spans the one second chunk boundary, so dropped or
            // doubled samples at the FIFO seam would show up here. A .5
            // amplitude sine has an RMS of .5/sqrt(2) ~= .354; the bounds
            // allow roughly +/-4dB of codec variance.
            try
            {
                _print("_audio: default codec round trip");
                const ftk::Path path(
                    (_getTempDir() / "FFmpegAudioDefault.mov").u8string());
                IOOptions options;
                options["FFmpeg/Codec"] = "mjpeg";
                writeMovie(path, options);

                auto read = readPlugin->audioRead(path);
                const auto ioInfo = read->getInfo().get();
                if (!ioInfo.audio.isValid() ||
                    ioInfo.audio.channelCount != audioInfo.channelCount ||
                    ioInfo.audio.sampleRate != audioInfo.sampleRate)
                {
                    _error("Default codec: unexpected audio info");
                    FTK_ASSERT(false);
                }
                const int64_t readSampleCount = static_cast<int64_t>(
                    ioInfo.audioTime.duration().
                        rescaled_to(audioInfo.sampleRate).value());
                if (std::abs(
                    readSampleCount - static_cast<int64_t>(sampleCount)) >
                    4096)
                {
                    std::stringstream ss;
                    ss << "Default codec: unexpected sample count: " <<
                        readSampleCount << ", expected: " << sampleCount;
                    _error(ss.str());
                    FTK_ASSERT(false);
                }

                // RMS over the middle second, [.25s, 1.25s).
                const size_t rmsOffset = 11025;
                const size_t rmsCount = 44100;
                const auto audioData = read->readAudio(
                    OTIO_NS::TimeRange(
                        ioInfo.audioTime.start_time() +
                            OTIO_NS::RationalTime(
                                static_cast<double>(rmsOffset),
                                audioInfo.sampleRate),
                        OTIO_NS::RationalTime(
                            static_cast<double>(rmsCount),
                            audioInfo.sampleRate))).get();
                if (!audioData.audio)
                {
                    _error("Default codec: no audio data");
                    FTK_ASSERT(false);
                }
                else
                {
                    double sum = 0.0;
                    const size_t count =
                        audioData.audio->getSampleCount() *
                        audioData.audio->getChannelCount();
                    switch (audioData.audio->getType())
                    {
                    case AudioType::F32:
                    {
                        const float* data = reinterpret_cast<const float*>(
                            audioData.audio->getData());
                        for (size_t i = 0; i < count; ++i)
                        {
                            sum += data[i] * data[i];
                        }
                        break;
                    }
                    case AudioType::S16:
                    {
                        const int16_t* data =
                            reinterpret_cast<const int16_t*>(
                                audioData.audio->getData());
                        for (size_t i = 0; i < count; ++i)
                        {
                            const double v = data[i] / 32767.0;
                            sum += v * v;
                        }
                        break;
                    }
                    default:
                        _error("Default codec: unexpected audio type");
                        FTK_ASSERT(false);
                        break;
                    }
                    if (count > 0)
                    {
                        const double rms = std::sqrt(sum / count);
                        if (rms < .2 || rms > .5)
                        {
                            std::stringstream ss;
                            ss << "Default codec: unexpected RMS: " << rms;
                            _error(ss.str());
                            FTK_ASSERT(false);
                        }
                    }
                }
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }

            // An explicitly requested audio codec that does not exist is an
            // error.
            {
                _print("_audio: bad codec error handling");
                const ftk::Path path(
                    (_getTempDir() / "FFmpegAudioBadCodec.mov").u8string());
                IOOptions options;
                options["FFmpeg/Codec"] = "mjpeg";
                options["FFmpeg/AudioCodec"] = "not_a_codec";
                IOInfo info;
                info.video.push_back(imageInfo);
                info.videoTime = OTIO_NS::TimeRange(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(
                        static_cast<double>(frameCount), 24.0));
                info.audio = audioInfo;
                info.audioTime = OTIO_NS::TimeRange(
                    OTIO_NS::RationalTime(0.0, audioInfo.sampleRate),
                    OTIO_NS::RationalTime(
                        static_cast<double>(sampleCount),
                        audioInfo.sampleRate));
                bool caught = false;
                try
                {
                    writePlugin->write(path, info, options);
                }
                catch (const std::exception&)
                {
                    caught = true;
                }
                if (!caught)
                {
                    _error("Bad codec: expected an exception");
                    FTK_ASSERT(false);
                }
            }

            // A container with no audio support skips the audio with a
            // warning instead of failing; writeAudio() becomes a no-op and
            // the video is still written.
            try
            {
                _print("_audio: container without audio support");
                const ftk::Path path(
                    (_getTempDir() / "FFmpegAudioSkip.mjpeg").u8string());
                IOOptions options;
                options["FFmpeg/Codec"] = "mjpeg";
                writeMovie(path, options);

                auto videoRead = readPlugin->videoRead(path);
                if (videoRead->getInfo().get().video.empty())
                {
                    _error("Skip audio: expected video");
                    FTK_ASSERT(false);
                }
                auto audioRead = readPlugin->audioRead(path);
                if (audioRead->getInfo().get().audio.isValid())
                {
                    _error("Skip audio: expected no audio");
                    FTK_ASSERT(false);
                }

                // A readAudio() request on a file without an audio stream
                // must complete with empty data rather than hanging or
                // hitting undefined behavior in the sample rate math.
                auto audioFuture = audioRead->readAudio(
                    OTIO_NS::TimeRange(
                        OTIO_NS::RationalTime(0.0, 44100.0),
                        OTIO_NS::RationalTime(44100.0, 44100.0)));
                if (audioFuture.wait_for(std::chrono::seconds(30)) !=
                    std::future_status::ready)
                {
                    _error("Skip audio: readAudio() timed out");
                    FTK_ASSERT(false);
                }
                else
                {
                    const auto audioData = audioFuture.get();
                    if (audioData.audio && audioData.audio->isValid())
                    {
                        _error("Skip audio: expected empty audio data");
                        FTK_ASSERT(false);
                    }
                }
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }
        }

        void FFmpegTest::_split()
        {
            // The video and audio readers have to work on their own, each
            // reporting only its own half of the information: merging the two
            // is the caller's job. A silent movie is the case worth checking
            // -- its audio reader has no stream to serve, and a request must
            // still come back.
            auto readSystem = _context->getSystem<ReadSystem>();
            auto readPlugin = readSystem->getPlugin<ffmpeg::ReadPlugin>();
            auto writeSystem = _context->getSystem<WriteSystem>();
            auto writePlugin = writeSystem->getPlugin<ffmpeg::WritePlugin>();

            const AudioInfo audioInfo(2, AudioType::S16, 44100);
            const size_t sampleCount = 44100;
            const size_t frameCount = 24;
            const auto imageInfo = writePlugin->getInfo(
                ftk::ImageInfo(ftk::Size2I(80, 60), ftk::ImageType::RGB_U8));
            auto image = ftk::Image::create(imageInfo);
            image->zero();
            const OTIO_NS::TimeRange videoTime(
                OTIO_NS::RationalTime(0.0, 24.0),
                OTIO_NS::RationalTime(static_cast<double>(frameCount), 24.0));
            const OTIO_NS::TimeRange audioTime(
                OTIO_NS::RationalTime(0.0, audioInfo.sampleRate),
                OTIO_NS::RationalTime(
                    static_cast<double>(sampleCount), audioInfo.sampleRate));
            IOOptions options;
            options["FFmpeg/Codec"] = "mjpeg";

            try
            {
                _print("_split: video and audio");
                const ftk::Path path(
                    (_getTempDir() / "FFmpegSplit.mov").u8string());
                {
                    IOInfo info;
                    info.video.push_back(imageInfo);
                    info.videoTime = videoTime;
                    info.audio = audioInfo;
                    info.audioTime = audioTime;
                    IOOptions writeOptions = options;
                    writeOptions["FFmpeg/AudioCodec"] = "pcm_s16le";
                    auto write = writePlugin->write(path, info, writeOptions);
                    for (size_t i = 0; i < frameCount; ++i)
                    {
                        write->writeVideo(
                            OTIO_NS::RationalTime(
                                static_cast<double>(i), 24.0),
                            image);
                    }
                    write->writeAudio(
                        audioTime,
                        makeSine(audioInfo, sampleCount, 0));
                    write->finish();
                }

                auto videoRead = readPlugin->videoRead(path, options);
                FTK_ASSERT(videoRead);
                const auto videoInfo = videoRead->getInfo().get();
                if (videoInfo.video.empty() || videoInfo.audio.isValid())
                {
                    _error("Split: unexpected video reader info");
                    FTK_ASSERT(false);
                }
                const auto videoData = videoRead->readVideo(
                    videoInfo.videoTime.start_time()).get();
                if (!videoData.image)
                {
                    _error("Split: expected video data");
                    FTK_ASSERT(false);
                }

                auto audioRead = readPlugin->audioRead(path, options);
                FTK_ASSERT(audioRead);
                const auto audioReadInfo = audioRead->getInfo().get();
                if (!audioReadInfo.audio.isValid() ||
                    !audioReadInfo.video.empty())
                {
                    _error("Split: unexpected audio reader info");
                    FTK_ASSERT(false);
                }
                const auto audioData = audioRead->readAudio(
                    OTIO_NS::TimeRange(
                        audioReadInfo.audioTime.start_time(),
                        OTIO_NS::RationalTime(
                            1000.0, audioInfo.sampleRate))).get();
                if (!audioData.audio ||
                    audioData.audio->getSampleCount() != 1000)
                {
                    _error("Split: expected audio data");
                    FTK_ASSERT(false);
                }
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }

            try
            {
                _print("_split: silent movie");
                const ftk::Path path(
                    (_getTempDir() / "FFmpegSplitSilent.mov").u8string());
                write(
                    writePlugin,
                    image,
                    path,
                    imageInfo,
                    ftk::ImageTags(),
                    videoTime.duration(),
                    options);

                auto audioRead = readPlugin->audioRead(path, options);
                FTK_ASSERT(audioRead);
                const auto info = audioRead->getInfo().get();
                if (info.audio.isValid())
                {
                    _error("Split: expected no audio");
                    FTK_ASSERT(false);
                }
                auto future = audioRead->readAudio(audioTime);
                if (future.wait_for(std::chrono::seconds(30)) !=
                    std::future_status::ready)
                {
                    _error("Split: readAudio() timed out");
                    FTK_ASSERT(false);
                }
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }
        }
    }
}

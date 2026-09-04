// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelineTest/TimelineTest.h>

#include <tlRender/Timeline/Timeline.h>
#include <tlRender/Timeline/Util.h>

#include <tlRender/IO/System.h>

#include <tlRender/IO/Plugin.h>
#include <tlRender/IO/SeqDecode.h>
#include <tlRender/IO/SeqIO.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Context.h>

#include <algorithm>
#include <cstring>
#include <ftk/Core/Format.h>
#include <ftk/Core/Path.h>

#include <opentimelineio/clip.h>
#include <opentimelineio/externalReference.h>
#include <opentimelineio/imageSequenceReference.h>
#include <opentimelineio/timeline.h>

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <thread>

namespace tl
{
    namespace timeline_tests
    {
        namespace
        {
            // Whether any plugin can read the given sample media. Several of
            // these tests need real pixels or the resolution behind them, and
            // a build configured without image formats has neither.
            bool canRead(
                const std::shared_ptr<ftk::Context>& context,
                const std::string& fileName)
            {
                return context->getSystem<ReadSystem>()->getPlugin(
                    ftk::Path(TLRENDER_SAMPLE_DATA, fileName)) != nullptr;
            }
        }

        TimelineTest::TimelineTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "timeline_tests::TimelineTest")
        {}

        std::shared_ptr<TimelineTest> TimelineTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<TimelineTest>(new TimelineTest(context));
        }

        void TimelineTest::run()
        {
            _enums();
            _options();
            _util();
            _transitions();
            _videoData();
            _timeline();
            _synchronous();
            _media();
            _readers();
            _memLifetime();
            _shutdown();
            _path();
            _seqOnDisk();
            _separateAudio();
            _spatial();
            _mediaReferences();
        }

        void TimelineTest::_spatial()
        {
            // Unit-less coordinates are scaled to pixels by the media's own
            // resolution, so a build that cannot read the media has no scale
            // to apply and lays out a canvas of a few pixels.
            if (!canRead(_context, "SpatialLarge.png"))
            {
                _print("Skipped: no plugin reads the spatial media");
                return;
            }

            // The fixtures pair a 1920x1080 clip with a 640x360 one. The
            // expected boxes are what each set of OTIO spatial coordinates
            // works out to in image space, after being scaled from unit-less
            // coordinates to pixels, flipped from Y-up to Y-down, and moved so
            // the canvas starts at the origin.
            const ftk::Box2F full(ftk::V2F(0.F, 0.F), ftk::V2F(1920.F, 1080.F));
            struct Case
            {
                std::string fileName;
                ftk::Size2I canvasSize;
                ftk::Box2F first;
                ftk::Box2F second;
            };
            const std::vector<Case> cases =
            {
                // The same box written three different ways. All three must
                // give the same result, and both clips must fill the canvas
                // whatever resolution they were rendered at.
                { "SpatialCoordinates.otio", ftk::Size2I(1920, 1080), full, full },
                { "SpatialCoordinatesUnits.otio", ftk::Size2I(1920, 1080), full, full },
                { "SpatialCoordinatesCentered.otio", ftk::Size2I(1920, 1080), full, full },

                // Side by side on a wider canvas.
                { "SpatialCoordinatesSideBySide.otio", ftk::Size2I(3840, 1080),
                  full,
                  ftk::Box2F(ftk::V2F(1920.F, 0.F), ftk::V2F(3840.F, 1080.F)) },

                // OTIO is Y-up, so "Upper" ends up at the top of the canvas
                // and "Lower", the first clip, at the bottom.
                { "SpatialCoordinatesOffsetY.otio", ftk::Size2I(1920, 2160),
                  ftk::Box2F(ftk::V2F(0.F, 1080.F), ftk::V2F(1920.F, 2160.F)),
                  full }
            };

            for (const auto& i : cases)
            {
                try
                {
                    const ftk::Path path(TLRENDER_SAMPLE_DATA, i.fileName);
                    _print(ftk::Format("Path: {0}").arg(path.get()));
                    auto timeline = Timeline::create(_context, path);

                    // The clips are 24 frames each, so these land one in each.
                    const std::vector<std::pair<double, ftk::Box2F> > frames =
                    {
                        { 0.0, i.first },
                        { 30.0, i.second }
                    };
                    for (const auto& frame : frames)
                    {
                        auto request = timeline->getVideo(
                            OTIO_NS::RationalTime(frame.first, 24.0));
                        const VideoFrame videoFrame = request.future.get();
                        FTK_CHECK(i.canvasSize == videoFrame.canvasSize);
                        FTK_CHECK(!videoFrame.layers.empty());
                        FTK_CHECK(videoFrame.layers[0].bounds.has_value());
                        FTK_CHECK(frame.second == videoFrame.layers[0].bounds.value());
                    }
                }
                catch (const std::exception& e)
                {
                    _error(e.what());
                }
            }

            // A clip whose media cannot be read must contribute no audio
            // layer at all, not an empty one. An empty layer reaches the
            // player as a stream that never produces a sample, and playback
            // is timed by the audio, so the clock stops and the video stops
            // with it: MissingMedia.otio would not play from its start, while
            // scrubbing still worked.
            try
            {
                const ftk::Path path(TLRENDER_SAMPLE_DATA, "MissingMedia.otio");
                _print(ftk::Format("Path: {0}").arg(path.get()));
                auto timeline = Timeline::create(_context, path);

                // The first second, where the audio track's clip has no media.
                auto request = timeline->getAudio(0);
                const AudioFrame frame = request.future.get();
                for (const auto& layer : frame.layers)
                {
                    FTK_CHECK(layer.audio);
                }

                // The second half has audio, so it still arrives.
                auto request2 = timeline->getAudio(4);
                const AudioFrame frame2 = request2.future.get();
                FTK_CHECK(!frame2.layers.empty());
                for (const auto& layer : frame2.layers)
                {
                    FTK_CHECK(layer.audio);
                }
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }

            // Timelines without spatial coordinates are laid out from the
            // image sizes as before.
            try
            {
                const ftk::Path path(TLRENDER_SAMPLE_DATA, "MultipleClips.otio");
                _print(ftk::Format("Path: {0}").arg(path.get()));
                auto timeline = Timeline::create(_context, path);
                auto request = timeline->getVideo(OTIO_NS::RationalTime(0.0, 24.0));
                const VideoFrame videoFrame = request.future.get();
                FTK_CHECK(!videoFrame.canvasSize.isValid());
                for (const auto& layer : videoFrame.layers)
                {
                    FTK_CHECK(!layer.bounds.has_value());
                }
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }

            // The pixels per unit must come from the first clip that has
            // spatial coordinates, not simply the first clip. Taking it from a
            // clip without them leaves the scale at 1 and reads the unit-less
            // coordinates as pixels, which gives a canvas of a few pixels.
            try
            {
                const ftk::Path path(TLRENDER_SAMPLE_DATA, "SpatialCoordinatesSecondClip.otio");
                _print(ftk::Format("Path: {0}").arg(path.get()));
                auto timeline = Timeline::create(_context, path);
                auto request = timeline->getVideo(OTIO_NS::RationalTime(30.0, 24.0));
                const VideoFrame videoFrame = request.future.get();
                FTK_CHECK(ftk::Size2I(1920, 1080) == videoFrame.canvasSize);
                FTK_CHECK(!videoFrame.layers.empty());
                FTK_CHECK(videoFrame.layers[0].bounds.has_value());
                FTK_CHECK(full == videoFrame.layers[0].bounds.value());
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }

            // Spatial::None ignores the coordinates even when they are there.
            try
            {
                const ftk::Path path(TLRENDER_SAMPLE_DATA, "SpatialCoordinates.otio");
                _print(ftk::Format("Path: {0}").arg(path.get()));
                Options options;
                options.spatial = Spatial::None;
                auto timeline = Timeline::create(_context, path, options);
                auto request = timeline->getVideo(OTIO_NS::RationalTime(0.0, 24.0));
                const VideoFrame videoFrame = request.future.get();
                FTK_CHECK(!videoFrame.canvasSize.isValid());
                for (const auto& layer : videoFrame.layers)
                {
                    FTK_CHECK(!layer.bounds.has_value());
                }
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }

            // Only the first clip of this timeline has spatial coordinates.
            // The default leaves the second clip alone, so it is laid out at
            // its own resolution; Spatial::Normalize gives it the reference
            // size so that both clips are displayed at the same size.
            try
            {
                const ftk::Path path(TLRENDER_SAMPLE_DATA, "SpatialCoordinatesPartial.otio");
                _print(ftk::Format("Path: {0}").arg(path.get()));
                {
                    auto timeline = Timeline::create(_context, path);
                    auto request = timeline->getVideo(OTIO_NS::RationalTime(30.0, 24.0));
                    const VideoFrame videoFrame = request.future.get();
                    FTK_CHECK(!videoFrame.layers.empty());
                    FTK_CHECK(!videoFrame.layers[0].bounds.has_value());
                }
                {
                    Options options;
                    options.spatial = Spatial::Normalize;
                    auto timeline = Timeline::create(_context, path, options);
                    auto request = timeline->getVideo(OTIO_NS::RationalTime(30.0, 24.0));
                    const VideoFrame videoFrame = request.future.get();
                    FTK_CHECK(ftk::Size2I(1920, 1080) == videoFrame.canvasSize);
                    FTK_CHECK(!videoFrame.layers.empty());
                    FTK_CHECK(videoFrame.layers[0].bounds.has_value());
                    FTK_CHECK(full == videoFrame.layers[0].bounds.value());
                }
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }
        }

        void TimelineTest::_enums()
        {
            FTK_TEST_ENUM(ImageSeqAudio);
            FTK_TEST_ENUM(Transition);
        }

        void TimelineTest::_options()
        {
            Options a;
            a.imageSeqAudio = ImageSeqAudio::FileName;
            FTK_CHECK(a == a);
            FTK_CHECK(a != Options());

            // The cache sizes are options rather than constants so that a
            // caller can ask for what it needs; comparing them is what keeps
            // a timeline from being reused with somebody else's sizes.
            Options cache;
            cache.readCacheMax = 1;
            FTK_CHECK(cache != Options());
            Options seq;
            seq.seqCacheMax = 1;
            FTK_CHECK(seq != Options());
        }

        void TimelineTest::_util()
        {
        }

        void TimelineTest::_transitions()
        {
            {
                FTK_CHECK(toTransition(std::string()) == Transition::None);
                FTK_CHECK(toTransition("SMPTE_Dissolve") == Transition::Dissolve);
            }
        }

        void TimelineTest::_videoData()
        {
            {
                VideoLayer a, b;
                FTK_CHECK(a == b);
                a.transition = Transition::Dissolve;
                FTK_CHECK(a != b);
            }
            {
                VideoData a, b;
                FTK_CHECK(a == b);
                a.time = OTIO_NS::RationalTime(1.0, 24.0);
                FTK_CHECK(a != b);
            }
        }

        void TimelineTest::_timeline()
        {
            // Test timelines.
            const std::vector<ftk::Path> paths =
            {
                ftk::Path(TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.0001.jpg"),
#if defined(TLRENDER_FFMPEG_PLUGIN)
                ftk::Path(TLRENDER_SAMPLE_DATA, "BART_2021-02-07.m4v"),
                ftk::Path(TLRENDER_SAMPLE_DATA, "MovieAndSeq.otio"),
                ftk::Path(TLRENDER_SAMPLE_DATA, "TransitionGap.otio"),
#endif // TLRENDER_FFMPEG_PLUGIN
#if defined(TLRENDER_FFMPEG_PLUGIN)
                ftk::Path(TLRENDER_SAMPLE_DATA, "SingleClip.otioz"),
#endif // TLRENDER_FFMPEG_PLUGIN
                ftk::Path(TLRENDER_SAMPLE_DATA, "SingleClipSeq.otioz"),
                // Written to a stream, so every entry carries a data
                // descriptor between its data and the next header.
                ftk::Path(TLRENDER_SAMPLE_DATA, "StreamedSeq.otioz"),
                // Compressed entries between the stored ones, so an entry's
                // data does not end where the next usable entry begins.
                ftk::Path(TLRENDER_SAMPLE_DATA, "MixedSeq.otioz")
            };
            for (const auto& path : paths)
            {
                try
                {
                    _print(ftk::Format("Timeline: {0}").arg(path.get()));
                    auto timeline = Timeline::create(_context, path);
                    _timeline(timeline);
                }
                catch (const std::exception& e)
                {
                    _error(e.what());
                }
            }
        }

        void TimelineTest::_timeline(const std::shared_ptr<Timeline>& timeline)
        {
            // Get video from the timeline.
            const OTIO_NS::TimeRange& timeRange = timeline->getTimeRange();
            std::vector<VideoFrame> videoFrame;
            std::vector<VideoRequest> videoRequests;
            for (size_t i = 0; i < static_cast<size_t>(timeRange.duration().value()); ++i)
            {
                videoRequests.push_back(timeline->getVideo(OTIO_NS::RationalTime(i, 24.0)));
            }
            IOOptions ioOptions;
            ioOptions["Layer"] = "1";
            for (size_t i = 0; i < static_cast<size_t>(timeRange.duration().value()); ++i)
            {
                videoRequests.push_back(timeline->getVideo(OTIO_NS::RationalTime(i, 24.0), ioOptions));
            }
            while (videoFrame.size() < static_cast<size_t>(timeRange.duration().value()) * 2)
            {
                auto i = videoRequests.begin();
                while (i != videoRequests.end())
                {
                    if (i->future.valid() &&
                        i->future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                    {
                        videoFrame.push_back(i->future.get());
                        i = videoRequests.erase(i);
                    }
                    else
                    {
                        ++i;
                    }
                }
            }
            FTK_CHECK(videoRequests.empty());

            // Get audio from the timeline.
            std::vector<AudioFrame> audioFrame;
            std::vector<AudioRequest> audioRequests;
            for (size_t i = 0; i < static_cast<size_t>(timeRange.duration().rescaled_to(1.0).value()); ++i)
            {
                audioRequests.push_back(timeline->getAudio(i));
            }
            while (audioFrame.size() < static_cast<size_t>(timeRange.duration().rescaled_to(1.0).value()))
            {
                auto i = audioRequests.begin();
                while (i != audioRequests.end())
                {
                    if (i->future.valid() &&
                        i->future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                    {
                        audioFrame.push_back(i->future.get());
                        i = audioRequests.erase(i);
                    }
                    else
                    {
                        ++i;
                    }
                }
            }
            FTK_CHECK(audioRequests.empty());

            // Cancel requests.
            videoFrame.clear();
            videoRequests.clear();
            audioFrame.clear();
            audioRequests.clear();
            for (size_t i = 0; i < static_cast<size_t>(timeRange.duration().value()); ++i)
            {
                videoRequests.push_back(timeline->getVideo(OTIO_NS::RationalTime(i, 24.0)));
            }
            for (size_t i = 0; i < static_cast<size_t>(timeRange.duration().value()); ++i)
            {
                videoRequests.push_back(timeline->getVideo(OTIO_NS::RationalTime(i, 24.0), ioOptions));
            }
            for (size_t i = 0; i < static_cast<size_t>(timeRange.duration().rescaled_to(1.0).value()); ++i)
            {
                audioRequests.push_back(timeline->getAudio(i));
            }
            std::vector<uint64_t> ids;
            for (const auto& i : videoRequests)
            {
                ids.push_back(i.id);
            }
            for (const auto& i : audioRequests)
            {
                ids.push_back(i.id);
            }
            timeline->cancelRequests(ids);
        }

        void TimelineTest::_readers()
        {
            // What the split of the readers is for: a movie with no audio
            // costs one reader, not two. The count is of live I/O objects,
            // so it goes up by one per reader the timeline holds.
            auto writeSystem = _context->getSystem<WriteSystem>();
            auto readSystem = _context->getSystem<ReadSystem>();
            const ftk::Path silentPath(
                ftk::fromFileSystem(_getTempDir() / "TimelineSilent.mov"));
            const ftk::Path audioPath(
                ftk::fromFileSystem(_getTempDir() / "TimelineAudio.mov"));
            auto writePlugin = writeSystem->getPlugin(silentPath);
            if (!writePlugin || !readSystem->getPlugin(silentPath))
            {
                // A build with no movie plugin cannot write one or read it
                // back.
                return;
            }

            // Named codecs, since the container default is not in every
            // FFmpeg build; a plugin that does not know these options
            // ignores them.
            IOOptions writeOptions;
            writeOptions["FFmpeg/Codec"] = "mjpeg";
            writeOptions["FFmpeg/AudioCodec"] = "pcm_s16le";

            const size_t frameCount = 24;
            const AudioInfo audioInfo(2, AudioType::S16, 44100);
            const OTIO_NS::TimeRange videoTime(
                OTIO_NS::RationalTime(0.0, 24.0),
                OTIO_NS::RationalTime(static_cast<double>(frameCount), 24.0));
            const ftk::ImageInfo imageInfo = writePlugin->getInfo(
                ftk::ImageInfo(ftk::Size2I(80, 60), ftk::ImageType::RGB_U8),
                writeOptions);
            auto image = ftk::Image::create(imageInfo);
            image->zero();
            const auto writeMovie = [&](const ftk::Path& path, bool audio)
            {
                IOInfo info;
                info.video.push_back(imageInfo);
                info.videoTime = videoTime;
                if (audio)
                {
                    info.audio = audioInfo;
                    info.audioTime = OTIO_NS::TimeRange(
                        OTIO_NS::RationalTime(0.0, audioInfo.sampleRate),
                        OTIO_NS::RationalTime(
                            static_cast<double>(audioInfo.sampleRate),
                            audioInfo.sampleRate));
                }
                auto write = writePlugin->write(path, info, writeOptions);
                for (size_t i = 0; i < frameCount; ++i)
                {
                    write->writeVideo(
                        OTIO_NS::RationalTime(static_cast<double>(i), 24.0),
                        image);
                }
                if (audio)
                {
                    auto samples = Audio::create(
                        audioInfo, audioInfo.sampleRate);
                    samples->zero();
                    write->writeAudio(*info.audioTime, samples);
                }
                write->finish();
            };
            try
            {
                writeMovie(silentPath, false);
                writeMovie(audioPath, true);
            }
            catch (const std::exception& e)
            {
                // The build has a movie plugin that cannot write this; there
                // is nothing to count.
                _print(ftk::Format("Cannot write a movie: {0}").arg(e.what()));
                return;
            }

            Options options;
            options.threaded = false;
            const auto readerCount = [&](const ftk::Path& path)
            {
                const size_t before = IIO::getObjectCount();
                auto timeline = Timeline::create(_context, path, options);
                // Read a frame as well: a reader the timeline only needs to
                // play would be created here rather than at open.
                auto future = timeline->readMedia(
                    path, timeline->getTimeRange().start_time());
                if (future.valid())
                {
                    future.get();
                }
                return IIO::getObjectCount() - before;
            };

            const size_t silentCount = readerCount(silentPath);
            _print(ftk::Format("Silent movie readers: {0}").arg(silentCount));
            if (silentCount != 1)
            {
                _fail(ftk::Format(
                    "A movie with no audio should cost one reader, not {0}").
                    arg(silentCount));
            }
            const size_t audioCount = readerCount(audioPath);
            _print(ftk::Format("Movie with audio readers: {0}").arg(audioCount));
            if (audioCount != 2)
            {
                _fail(ftk::Format(
                    "A movie with audio should cost two readers, not {0}").
                    arg(audioCount));
            }
        }

        void TimelineTest::_memLifetime()
        {
            // A bundle's media is a range of bytes inside the archive, which
            // is mapped into memory once and read from there.
            const ftk::Path path(TLRENDER_SAMPLE_DATA, "SingleClipSeq.otioz");
            Options options;
            options.threaded = false;
            auto timeline = Timeline::create(_context, path, options);
            const auto clips = timeline->getOTIOTimeline()->find_clips();
            FTK_CHECK(!clips.empty());
            auto mediaReference = clips[0]->media_reference();
            const auto mediaPath = getPath(
                mediaReference,
                timeline->getPath().getDir(),
                ftk::PathOptions());
            const auto plugin =
                _context->getSystem<ReadSystem>()->getPlugin(mediaPath);
            if (!plugin)
            {
                _print("Skipped: no plugin reads the bundle's media");
                return;
            }
            const auto decode = plugin->decode(IOOptions());
            FTK_CHECK(decode);

            auto mem = timeline->getMem(mediaReference);
            FTK_CHECK(!mem.empty());
            auto seq = SeqDecode::create(mediaPath, mem, decode);
            const IOInfo info = seq->getInfo();
            FTK_CHECK(!info.video.empty());
            const auto time = info.videoTime->start_time();
            FTK_CHECK(seq->readVideo(time).image);

            // The decoder was handed the memory the frames live in, and each
            // of those keeps the mapping open, so nothing it needs belongs to
            // the timeline any more. Anything that holds readers beyond the
            // timeline that opened them -- a pool shared between timelines,
            // say -- rests on exactly this.
            //
            // Both of the other holders have to go first or this proves
            // nothing: the copy here would keep the archive mapped by itself,
            // which is how the first version of this test passed even with
            // the decoder's keepalive taken away.
            mem.clear();
            timeline.reset();
            const VideoData data = seq->readVideo(time);
            FTK_CHECK(data.image);
            FTK_CHECK(data.image->getSize() == info.video[0].size);

            _print("a decoder outlives the timeline that opened it");
        }

        void TimelineTest::_shutdown()
        {
            _print("Shutdown");
            // Destroying an idle timeline must wake its thread. The thread
            // waits for a request rather than polling for one, so a notify
            // that goes missing is not covered by the next poll: it costs
            // the whole logging interval, which is what this measures.
            auto timeline = Timeline::create(
                _context,
                ftk::Path(TLRENDER_SAMPLE_DATA, "MultipleClips.otio"));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            const auto t0 = std::chrono::steady_clock::now();
            timeline.reset();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t0).count();
            std::stringstream ss;
            ss << "Shutdown wake: " << elapsed << "ms";
            _print(ss.str());
            if (elapsed >= 1000)
            {
                _fail("Shutdown had to wait for the logging interval");
            }
        }

        void TimelineTest::_path()
        {
            // What a timeline was opened from, given back as it was given.
            // A sequence's range is the path's own state rather than part of
            // its name, so a path that goes out through a string comes back
            // naming one frame -- and callers reopen what getPath() gives
            // them.
            // Opening needs a reader, and the minimal configuration compiles
            // no image plugin at all.
            ftk::Path path(TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.0001.png");
            if (!_context->getSystem<ReadSystem>()->getPlugin(path))
            {
                _print("Skipped: no plugin reads the fixture");
                return;
            }
            path.setFrames(ftk::RangeI64(1, 3));
            Options options;
            auto timeline = Timeline::create(_context, path, options);
            _print(ftk::Format("Path: {0}").arg(timeline->getPath().get()));
            FTK_CHECK(timeline->getPath().isSeq());
            FTK_CHECK(ftk::RangeI64(1, 3) == timeline->getPath().getFrames().value());
        }

        void TimelineTest::_seqOnDisk()
        {
            // A sequence opened again covers the frames that are there then,
            // not the ones that were there before. This is what reloading a
            // render in progress does, and what taking frames away does to
            // it.
            auto readSystem = _context->getSystem<ReadSystem>();
            auto writeSystem = _context->getSystem<WriteSystem>();
            const std::filesystem::path dir = _getTempDir() / "SeqOnDisk";
            std::filesystem::remove_all(dir);
            std::filesystem::create_directory(dir);
            const auto frameFile = [&dir](int frame)
                {
                    return ftk::Path(ftk::fromFileSystem(dir / ftk::Format("render.{0}.png").
                        arg(frame, 4, '0').str()));
                };
            auto writePlugin = writeSystem->getPlugin(frameFile(1));
            if (!writePlugin || !readSystem->getPlugin(frameFile(1)))
            {
                _print("Skipped: no plugin reads the fixture");
                return;
            }
            IOInfo writeInfo;
            writeInfo.video.push_back(writePlugin->getInfo(
                ftk::ImageInfo(ftk::Size2I(16, 16), ftk::ImageType::RGB_U8)));
            const auto writeFrames = [&](const std::vector<int>& frames)
                {
                    for (int frame : frames)
                    {
                        auto write = writeSystem->write(frameFile(frame), writeInfo);
                        write->writeVideo(
                            OTIO_NS::RationalTime(static_cast<double>(frame), 24.0),
                            ftk::Image::create(writeInfo.video[0]));
                    }
                };

            // Opened the way an application opens: gathering the frames is
            // getPaths()' job, and it is what goes and looks.
            ftk::DirListOptions dirListOptions;
            dirListOptions.seqExts = getExts(
                _context, static_cast<int>(FileType::Seq));
            const auto open = [&]
                {
                    const auto paths = getPaths(
                        _context, frameFile(1), dirListOptions);
                    FTK_CHECK(1 == paths.size());
                    return paths.front();
                };

            writeFrames({ 1, 2, 3 });
            {
                const ftk::Path path = open();
                FTK_CHECK(ftk::RangeI64(1, 3) == path.getFrames().value());
                auto timeline = Timeline::create(_context, path);
                FTK_CHECK(3 == timeline->getTimeRange().duration().value());
            }

            // Frames rendered since.
            writeFrames({ 4, 5, 6 });
            {
                const ftk::Path path = open();
                FTK_CHECK(ftk::RangeI64(1, 6) == path.getFrames().value());
                auto timeline = Timeline::create(_context, path);
                FTK_CHECK(6 == timeline->getTimeRange().duration().value());
            }

            // Frames taken away from the middle: the range still spans them,
            // and the holes are the sequence's own.
            std::filesystem::remove(
                ftk::toFileSystem(frameFile(3).get()));
            std::filesystem::remove(
                ftk::toFileSystem(frameFile(4).get()));
            {
                const ftk::Path path = open();
                _print("Frames on disk: " + ftk::getLabel(path.getSeq()));
                FTK_CHECK(path.isPartialSeq());
                FTK_CHECK("1-2,5-6" == ftk::getLabel(path.getSeq()));
                auto timeline = Timeline::create(_context, path);
                FTK_CHECK(6 == timeline->getTimeRange().duration().value());
            }

            // And taken away from the end, where they shorten it.
            std::filesystem::remove(
                ftk::toFileSystem(frameFile(5).get()));
            std::filesystem::remove(
                ftk::toFileSystem(frameFile(6).get()));
            {
                const ftk::Path path = open();
                FTK_CHECK(ftk::RangeI64(1, 2) == path.getFrames().value());
                auto timeline = Timeline::create(_context, path);
                FTK_CHECK(2 == timeline->getTimeRange().duration().value());
            }
        }

        void TimelineTest::_separateAudio()
        {
#if defined(TLRENDER_FFMPEG_PLUGIN)
            try
            {
                const ftk::Path path(TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.0001.jpg");
                const ftk::Path audioPath(TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.wav");
                auto timeline = Timeline::create(_context, path.get(), audioPath.get());
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }
            try
            {
                const ftk::Path path(TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.0001.jpg");
                const ftk::Path audioPath(TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.wav");
                auto timeline = Timeline::create(_context, path, audioPath);
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }
            try
            {
                const ftk::Path path(TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.0001.jpg");
                _print(ftk::Format("Path: {0}").arg(path.get()));
                Options options;
                options.imageSeqAudio = ImageSeqAudio::None;
                auto timeline = Timeline::create(_context, path, options);
                const ftk::Path& audioPath = timeline->getAudioPath();
                FTK_CHECK(audioPath.isEmpty());
                _print(ftk::Format("Audio path: {0}").arg(audioPath.get()));
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }
            try
            {
                // A sequence: audio is looked for beside one, and a path
                // naming a single frame names a single frame.
                ftk::Path path(TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.0001.jpg");
                path.setFrames(ftk::RangeI64(1, 3));
                _print(ftk::Format("Path: {0}").arg(path.get()));
                Options options;
                options.imageSeqAudio = ImageSeqAudio::Ext;
                auto timeline = Timeline::create(_context, path, options);
                const ftk::Path& audioPath = timeline->getAudioPath();
                FTK_CHECK(!audioPath.isEmpty());
                _print(ftk::Format("Audio path: {0}").arg(audioPath.get()));
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }
            try
            {
                ftk::Path path(TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.0001.jpg");
                path.setFrames(ftk::RangeI64(1, 3));
                _print(ftk::Format("Path: {0}").arg(path.get()));
                Options options;
                options.imageSeqAudio = ImageSeqAudio::FileName;
                options.imageSeqAudioFileName = ftk::Path(
                    TLRENDER_SAMPLE_DATA, "AudioToneStereo.wav").get();
                auto timeline = Timeline::create(_context, path, options);
                const ftk::Path& audioPath = timeline->getAudioPath();
                FTK_CHECK(!audioPath.isEmpty());
                _print(ftk::Format("Audio path: {0}").arg(audioPath.get()));
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }

            // The same, by a relative path that already carries its range,
            // which is what listing a directory gives. expandSeq() searches
            // only when there is no range yet, and it is the search that
            // makes the path absolute, so this one stays relative all the way
            // to the media reference.
            try
            {
                const std::filesystem::path relative = std::filesystem::relative(
                    ftk::toFileSystem(TLRENDER_SAMPLE_DATA),
                    std::filesystem::current_path());
                if (!relative.empty())
                {
                    ftk::Path path(
                        ftk::fromFileSystem(relative),
                        "Seq/BART_2021-02-07.0001.jpg");
                    path.setFrames(ftk::RangeI64(1, 3));
                    FTK_CHECK(path.isSeq());
                    _print(ftk::Format("Path: {0}").arg(path.get()));
                    Options options;
                    options.imageSeqAudio = ImageSeqAudio::Ext;
                    auto timeline = Timeline::create(_context, path, options);
                    FTK_CHECK(!timeline->getAudioPath().isEmpty());
                    for (const auto& mediaPath : timeline->getMediaPaths())
                    {
                        _print(ftk::Format("Media path: {0}").arg(mediaPath.get()));
                        FTK_CHECK(std::filesystem::exists(
                            ftk::toFileSystem(mediaPath.getFileName(true))));
                    }
                }
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }
#endif // TLRENDER_FFMPEG_PLUGIN
        }

        void TimelineTest::_mediaReferences()
        {
            // Switching between media references is checked by the image that
            // comes back and the resolution it is laid out at, neither of
            // which a build without image formats can produce.
            if (!canRead(_context, "SpatialLarge.png"))
            {
                _print("Skipped: no plugin reads the media reference fixtures");
                return;
            }

            // A clip may carry several media references, for example a proxy
            // and a full resolution version of the same media.
            //
            // The first clip of this timeline has a "Proxy" and a "Full"
            // reference, both authored with the same spatial coordinates, and
            // is active on "Proxy". The second clip has only a default
            // reference, at the proxy resolution, and is there to exercise the
            // fallback.
            //
            // The timeline is deliberately active on the proxy, since the
            // canvas is built from the largest media reference rather than
            // from the active one; a timeline that opens on a proxy still
            // renders at the full resolution it can be switched to.
            const ftk::Size2I smallSize(640, 360);
            const ftk::Size2I largeSize(1920, 1080);
            const ftk::Size2I canvasSize(1920, 1080);
            try
            {
                const ftk::Path path(TLRENDER_SAMPLE_DATA, "MultipleMediaRefs.otio");
                _print(ftk::Format("Path: {0}").arg(path.get()));
                auto timeline = Timeline::create(_context, path);

                // Every key used anywhere in the timeline is listed, including
                // the default key of the clip that has a single reference.
                const auto keys = timeline->getMediaReferenceKeys();
                FTK_CHECK(3 == keys.size());
                FTK_CHECK(keys.end() != std::find(keys.begin(), keys.end(), "Full"));
                FTK_CHECK(keys.end() != std::find(keys.begin(), keys.end(), "Proxy"));
                FTK_CHECK(keys.end() != std::find(
                    keys.begin(), keys.end(), OTIO_NS::Clip::default_media_key));

                // Without a key the clips are read from the media reference
                // OTIO has active.
                FTK_CHECK(timeline->getMediaReferenceKey().empty());
                std::optional<ftk::Box2F> bounds;
                {
                    auto request = timeline->getVideo(OTIO_NS::RationalTime(0.0, 24.0));
                    const VideoFrame videoFrame = request.future.get();
                    FTK_CHECK(!videoFrame.layers.empty());
                    FTK_CHECK(videoFrame.layers[0].image);
                    FTK_CHECK(smallSize == videoFrame.layers[0].image->getSize());
                    // The proxy is being read, but the canvas is built for the
                    // full resolution reference that the clip can be switched
                    // to, so no resolution is lost by opening on the proxy.
                    FTK_CHECK(canvasSize == videoFrame.canvasSize);
                    bounds = videoFrame.layers[0].bounds;
                    FTK_CHECK(bounds.has_value());
                }

                // Switching to the full resolution reference changes the image
                // that is read. The canvas and the box the clip occupies
                // within it are unchanged, which is the point of the feature:
                // the resolution changes underneath a fixed layout.
                timeline->setMediaReferenceKey("Full");
                FTK_CHECK("Full" == timeline->getMediaReferenceKey());
                // The reported information follows the media reference being
                // read, so that it describes what is on screen.
                FTK_CHECK(!timeline->getIOInfo().video.empty());
                FTK_CHECK(largeSize == timeline->getIOInfo().video[0].size);
                {
                    auto request = timeline->getVideo(OTIO_NS::RationalTime(0.0, 24.0));
                    const VideoFrame videoFrame = request.future.get();
                    FTK_CHECK(!videoFrame.layers.empty());
                    FTK_CHECK(videoFrame.layers[0].image);
                    FTK_CHECK(largeSize == videoFrame.layers[0].image->getSize());
                    FTK_CHECK(canvasSize == videoFrame.canvasSize);
                    FTK_CHECK(bounds == videoFrame.layers[0].bounds);
                }

                // The second clip has no reference under this key, so it falls
                // back to the default media key and is left as it was.
                {
                    auto request = timeline->getVideo(OTIO_NS::RationalTime(30.0, 24.0));
                    const VideoFrame videoFrame = request.future.get();
                    FTK_CHECK(!videoFrame.layers.empty());
                    FTK_CHECK(videoFrame.layers[0].image);
                    FTK_CHECK(smallSize == videoFrame.layers[0].image->getSize());
                    FTK_CHECK(canvasSize == videoFrame.canvasSize);
                }

                // A key set for a single clip overrides the timeline wide key.
                const auto otioClips =
                    timeline->getOTIOTimeline().value->find_children<OTIO_NS::Clip>();
                FTK_CHECK(2 == otioClips.size());
                timeline->setMediaReferenceKey(otioClips[0], "Proxy");
                FTK_CHECK("Proxy" == timeline->getMediaReferenceKey(otioClips[0]));
                {
                    auto request = timeline->getVideo(OTIO_NS::RationalTime(0.0, 24.0));
                    const VideoFrame videoFrame = request.future.get();
                    FTK_CHECK(!videoFrame.layers.empty());
                    FTK_CHECK(videoFrame.layers[0].image);
                    FTK_CHECK(smallSize == videoFrame.layers[0].image->getSize());
                    FTK_CHECK(bounds == videoFrame.layers[0].bounds);
                }

                // Clearing the key for the clip returns it to the timeline
                // wide key.
                timeline->setMediaReferenceKey(otioClips[0], std::string());
                FTK_CHECK(timeline->getMediaReferenceKey(otioClips[0]).empty());
                {
                    auto request = timeline->getVideo(OTIO_NS::RationalTime(0.0, 24.0));
                    const VideoFrame videoFrame = request.future.get();
                    FTK_CHECK(!videoFrame.layers.empty());
                    FTK_CHECK(videoFrame.layers[0].image);
                    FTK_CHECK(largeSize == videoFrame.layers[0].image->getSize());
                }
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }

            // The same timeline as a bundle. Every media reference is mapped
            // to the memory it occupies in the bundle, not just the active
            // one, so that the active reference can be changed without
            // re-reading the file.
            //
            // Note that nothing here can be read from disk: the media only
            // exists inside the bundle, and "media/SpatialLarge.png" does not
            // resolve next to the sample data. Reading it at all proves it
            // came from the mapped memory.
            try
            {
                const ftk::Path path(TLRENDER_SAMPLE_DATA, "MultipleMediaRefs.otioz");
                _print(ftk::Format("Path: {0}").arg(path.get()));
                auto timeline = Timeline::create(_context, path);
                const auto otioClips =
                    timeline->getOTIOTimeline().value->find_children<OTIO_NS::Clip>();
                FTK_CHECK(2 == otioClips.size());

                // Both of the first clip's references are mapped, including
                // the one that is not active.
                const auto mediaReferences = otioClips[0]->media_references();
                FTK_CHECK(2 == mediaReferences.size());
                for (const auto& i : mediaReferences)
                {
                    FTK_CHECK(!timeline->getMem(i.second).empty());
                }

                // The bundle opens on the proxy and switches to the full
                // resolution reference without re-reading the file.
                {
                    auto request = timeline->getVideo(OTIO_NS::RationalTime(0.0, 24.0));
                    const VideoFrame videoFrame = request.future.get();
                    FTK_CHECK(!videoFrame.layers.empty());
                    FTK_CHECK(videoFrame.layers[0].image);
                    FTK_CHECK(smallSize == videoFrame.layers[0].image->getSize());
                    FTK_CHECK(canvasSize == videoFrame.canvasSize);
                }
                timeline->setMediaReferenceKey("Full");
                {
                    auto request = timeline->getVideo(OTIO_NS::RationalTime(0.0, 24.0));
                    const VideoFrame videoFrame = request.future.get();
                    FTK_CHECK(!videoFrame.layers.empty());
                    FTK_CHECK(videoFrame.layers[0].image);
                    FTK_CHECK(largeSize == videoFrame.layers[0].image->getSize());
                    FTK_CHECK(canvasSize == videoFrame.canvasSize);
                }
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }

            // A bundle carrying only the active media. It opens, since the
            // media it is playing is there, but the reference that is missing
            // cannot be used. It is not read from the file system: the bundle
            // names the media, so reading a file of that name from somewhere
            // else would not be the media the bundle describes.
            try
            {
                const ftk::Path path(TLRENDER_SAMPLE_DATA, "MissingMediaRef.otioz");
                _print(ftk::Format("Path: {0}").arg(path.get()));
                auto timeline = Timeline::create(_context, path);

                const auto otioClips =
                    timeline->getOTIOTimeline().value->find_children<OTIO_NS::Clip>();
                const auto mediaReferences = otioClips[0]->media_references();
                FTK_CHECK(2 == mediaReferences.size());
                FTK_CHECK(!timeline->getMem(mediaReferences.at("Proxy")).empty());
                FTK_CHECK(timeline->getMem(mediaReferences.at("Full")).empty());

                // The active reference still reads.
                {
                    auto request = timeline->getVideo(OTIO_NS::RationalTime(0.0, 24.0));
                    const VideoFrame videoFrame = request.future.get();
                    FTK_CHECK(!videoFrame.layers.empty());
                    FTK_CHECK(videoFrame.layers[0].image);
                    FTK_CHECK(smallSize == videoFrame.layers[0].image->getSize());
                }

                // The missing reference does not. Its path resolves to a file
                // that exists next to the sample data, so this would give an
                // image if the media were being taken from the file system.
                timeline->setMediaReferenceKey("Full");
                {
                    auto request = timeline->getVideo(OTIO_NS::RationalTime(0.0, 24.0));
                    const VideoFrame videoFrame = request.future.get();
                    FTK_CHECK(!videoFrame.layers.empty());
                    FTK_CHECK(!videoFrame.layers[0].image);
                }
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }

            // A bundle whose sequence is missing frames. Its content.otio
            // still describes all ninety, since an image sequence reference
            // cannot express gaps, so the frames the bundle does hold have to
            // stay addressable by frame number rather than being packed
            // together.
            try
            {
                const ftk::Path path(TLRENDER_SAMPLE_DATA, "PartialSeq.otioz");
                _print(ftk::Format("Path: {0}").arg(path.get()));
                auto timeline = Timeline::create(_context, path);

                const auto otioClips =
                    timeline->getOTIOTimeline().value->find_children<OTIO_NS::Clip>();
                const auto mediaReference = otioClips[0]->media_reference();
                const auto mem = timeline->getMem(mediaReference);

                // Indexed by frame number over the whole range, not by how
                // many frames are present.
                FTK_CHECK(90 == mem.size());
                const std::vector<size_t> missing = { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 49 };
                for (size_t i = 0; i < mem.size(); ++i)
                {
                    const bool present = std::find(
                        missing.begin(), missing.end(), i) == missing.end();
                    FTK_CHECK(present == (mem[i].p != nullptr));
                }

                // The frames the bundle holds are JPEG, which not every
                // configuration builds. What is above this does not decode
                // anything, so it is checked either way.
                if (_context->getSystem<ReadSystem>()->getPlugin(getPath(
                    mediaReference,
                    timeline->getPath().getDir(),
                    ftk::PathOptions())))
                {
                    // A frame the bundle holds reads. Frame 30 sits
                    // after the first gap, so it would come back as the
                    // wrong image if the frames had been packed.
                    for (double frame : { 0.0, 30.0, 89.0 })
                    {
                        auto request = timeline->getVideo(
                            OTIO_NS::RationalTime(frame, 30.0));
                        const VideoFrame videoFrame = request.future.get();
                        FTK_CHECK(!videoFrame.layers.empty());
                        FTK_CHECK(videoFrame.layers[0].image);
                    }

                    // A frame it does not hold follows the policy the
                    // bundle declares, which for this one is black. It is
                    // not read from the file system whatever the policy says.
                    {
                        auto request = timeline->getVideo(
                            OTIO_NS::RationalTime(12.0, 30.0));
                        const VideoFrame videoFrame = request.future.get();
                        FTK_CHECK(!videoFrame.layers.empty());
                        const auto& image = videoFrame.layers[0].image;
                        FTK_CHECK(image);
                        if (image)
                        {
                            std::vector<uint8_t> zero(image->getByteCount(), 0);
                            FTK_CHECK(0 == memcmp(
                                image->getData(), zero.data(), zero.size()));
                        }
                    }
                }
                else
                {
                    _print("Skipped: no plugin reads the bundle's media");
                }
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }

            // A frame that will not read has to reach the timeline's error
            // count under the error policy, rather than arriving as a blank
            // frame with nothing said about it.
            try
            {
                auto writeSystem = _context->getSystem<WriteSystem>();
                const ftk::Path path(
                    ftk::fromFileSystem(_getTempDir() / "TimelineGap.0001.png"));
                if (auto writePlugin = writeSystem->getPlugin(path))
                {
                    IOInfo writeInfo;
                    writeInfo.video.push_back(writePlugin->getInfo(
                        ftk::ImageInfo(
                            ftk::Size2I(16, 16), ftk::ImageType::RGB_U8)));
                    auto write = writeSystem->write(path, writeInfo);
                    // Frames 1 and 2 only, so 3 is missing.
                    for (int64_t frame : { 1, 2 })
                    {
                        write->writeVideo(
                            OTIO_NS::RationalTime(
                                static_cast<double>(frame), 24.0),
                            ftk::Image::create(writeInfo.video[0]));
                    }
                    write.reset();

                    ftk::Path seqPath(path);
                    seqPath.setFrames(ftk::RangeI64(1, 3));

                    Options options;
                    options.ioOptions["SeqIO/MissingFrames"] =
                        to_string(MissingFrames::Error);
                    auto timeline = Timeline::create(_context, seqPath, options);
                    FTK_CHECK(0 == timeline->getReadErrorCount());
                    auto request = timeline->getVideo(
                        OTIO_NS::RationalTime(3.0, 24.0));
                    request.future.get();
                    FTK_CHECK(timeline->getReadErrorCount() > 0);
                    FTK_CHECK(!timeline->getReadError().empty());
                    _print("Read error: " + timeline->getReadError());

                    // A range stated beyond the frames that exist, which is
                    // how a render in progress is opened. Nothing on disk
                    // changes; the timeline is simply the length the sequence
                    // is meant to be.
                    std::shared_ptr<Timeline> wideTimeline;
                    {
                        ftk::Path wide(path);
                        wide.setFrames(ftk::RangeI64(1, 100));
                        Options wideOptions;
                        wideOptions.ioOptions["SeqIO/MissingFrames"] =
                            to_string(MissingFrames::Black);
                        wideTimeline = Timeline::create(
                            _context, wide, wideOptions);
                        const auto timeRange = wideTimeline->getTimeRange();
                        FTK_CHECK(100 == timeRange.duration().value());

                        // A frame that exists, and one that does not.
                        auto have = wideTimeline->getVideo(
                            OTIO_NS::RationalTime(1.0, 24.0));
                        FTK_CHECK(have.future.get().layers[0].image);
                        auto pending = wideTimeline->getVideo(
                            OTIO_NS::RationalTime(80.0, 24.0));
                        FTK_CHECK(pending.future.get().layers[0].image);
                        FTK_CHECK(0 == wideTimeline->getReadErrorCount());
                    }

                    // A range of one frame survives being opened, as does a
                    // path naming a single frame: opening reads what it is
                    // given and never goes looking for the rest.
                    {
                        ftk::Path one(path);
                        one.setFrames(ftk::RangeI64(1, 1));
                        auto oneTimeline = Timeline::create(_context, one);
                        FTK_CHECK(
                            1 == oneTimeline->getTimeRange().duration().value());

                        auto bare = Timeline::create(_context, ftk::Path(path));
                        FTK_CHECK(
                            1 == bare->getTimeRange().duration().value());
                    }

                    // A structural policy answers a sparse sequence by
                    // building a clip over each run of frames that are there.
                    // The frames deliberately have a hole in the middle: runs
                    // that start at the beginning and run together would let a
                    // wrong mapping look right.
                    {
                        const ftk::Path sparsePath(
                            ftk::fromFileSystem(_getTempDir() / "TimelineSparse.0001.png"));
                        auto sparseWrite = writeSystem->write(
                            sparsePath, writeInfo);
                        for (int64_t frame : { 1, 2, 5, 6 })
                        {
                            sparseWrite->writeVideo(
                                OTIO_NS::RationalTime(
                                    static_cast<double>(frame), 24.0),
                                ftk::Image::create(writeInfo.video[0]));
                        }
                        sparseWrite.reset();

                        // Runs of 1-2 and 5-6 inside a range of 1-8.
                        ftk::Path statedPath(sparsePath);
                        statedPath.setFrames(ftk::RangeI64(1, 8));

                        // Skip puts the runs end to end, so the timeline is as
                        // long as the frames it has.
                        Options skipOptions;
                        skipOptions.ioOptions["SeqIO/MissingFrames"] =
                            to_string(MissingFrames::Skip);
                        auto skipTimeline = Timeline::create(
                            _context, statedPath, skipOptions);
                        FTK_CHECK(4 ==
                            skipTimeline->getTimeRange().duration().value());

                        // Each position names the frame it stands for and reads
                        // it. This is what the frame counter shows.
                        const std::vector<std::pair<double, int64_t> > skipped =
                        {
                            { 1.0, 1 }, { 2.0, 2 }, { 3.0, 5 }, { 4.0, 6 }
                        };
                        for (const auto& i : skipped)
                        {
                            const OTIO_NS::RationalTime time(i.first, 24.0);
                            const auto frame =
                                skipTimeline->getMediaFrame(time);
                            FTK_CHECK(frame.has_value());
                            FTK_CHECK(i.second == frame.value());
                            auto request = skipTimeline->getVideo(time);
                            FTK_CHECK(request.future.get().layers[0].image);
                        }
                        FTK_CHECK(0 == skipTimeline->getReadErrorCount());

                        // And the way back, which is what typing a frame number
                        // needs. A frame in the second run has to be found there
                        // rather than in the clip being looked at.
                        for (const auto& i : skipped)
                        {
                            const auto time = skipTimeline->getTimelineTime(
                                OTIO_NS::RationalTime(1.0, 24.0),
                                OTIO_NS::RationalTime(
                                    static_cast<double>(i.second), 24.0));
                            FTK_CHECK(time.has_value());
                            FTK_CHECK(i.first == time.value().value());
                        }

                        // A frame that is not there snaps to the last one
                        // before it, and one before them all to the first.
                        const std::vector<std::pair<int64_t, int64_t> > snap =
                        {
                            { 3, 2 },   // in the hole, so back to 2
                            { 4, 2 },
                            { 8, 6 },   // past the frames rendered so far
                            { 0, 1 }    // before the first, so up to it
                        };
                        for (const auto& i : snap)
                        {
                            const auto time = skipTimeline->getTimelineTime(
                                OTIO_NS::RationalTime(1.0, 24.0),
                                OTIO_NS::RationalTime(
                                    static_cast<double>(i.first), 24.0));
                            FTK_CHECK(time.has_value());
                            const auto frame =
                                skipTimeline->getMediaFrame(time.value());
                            FTK_CHECK(frame.has_value());
                            FTK_CHECK(i.second == frame.value());
                        }

                        // Gaps leaves the holes in, so the timeline is the range
                        // asked for and every frame keeps the time it had. The
                        // counter needs no mapping at all in this one.
                        Options gapsOptions;
                        gapsOptions.ioOptions["SeqIO/MissingFrames"] =
                            to_string(MissingFrames::Gaps);
                        auto gapsTimeline = Timeline::create(
                            _context, statedPath, gapsOptions);
                        FTK_CHECK(8 ==
                            gapsTimeline->getTimeRange().duration().value());
                        for (int64_t frame : { 1, 2, 5, 6 })
                        {
                            const OTIO_NS::RationalTime time(
                                static_cast<double>(frame), 24.0);
                            const auto at = gapsTimeline->getMediaFrame(time);
                            FTK_CHECK(at.has_value());
                            FTK_CHECK(frame == at.value());
                            auto request = gapsTimeline->getVideo(time);
                            FTK_CHECK(request.future.get().layers[0].image);
                        }
                        FTK_CHECK(0 == gapsTimeline->getReadErrorCount());

                        // A hole has no media, so there is no frame to name
                        // there and nothing is read for it.
                        for (int64_t frame : { 3, 4, 7, 8 })
                        {
                            FTK_CHECK(!gapsTimeline->getMediaFrame(
                                OTIO_NS::RationalTime(
                                    static_cast<double>(frame), 24.0)).
                                has_value());
                        }

                        // Both are one media played through, in runs rather
                        // than in one piece, so the frame numbers keep
                        // increasing and are worth showing in place of the
                        // timeline's own time.
                        FTK_CHECK(skipTimeline->isMediaTimeContinuous());
                        FTK_CHECK(gapsTimeline->isMediaTimeContinuous());
                    }

                    // Without a structural policy a timeline time is the frame
                    // number.
                    {
                        const auto frame = wideTimeline->getMediaFrame(
                            OTIO_NS::RationalTime(80.0, 24.0));
                        FTK_CHECK(frame.has_value());
                        FTK_CHECK(80 == frame.value());
                    }

                    // Skip over a sequence whose frames are all there.
                    // One run rather than several, so the timeline is the
                    // single clip a complete sequence gets rather than one
                    // clip per run, and it still covers every frame.
                    {
                        const ftk::Path wholePath(
                            TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.0001.jpg");
                        Options wholeOptions;
                        wholeOptions.ioOptions["SeqIO/MissingFrames"] =
                            to_string(MissingFrames::Skip);
                        auto whole = Timeline::create(
                            _context, wholePath, wholeOptions);
                        const OTIO_NS::TimeRange range = whole->getTimeRange();
                        _print(ftk::Format("Whole sequence skipped: {0}").
                            arg(range));
                        FTK_CHECK(range.duration().value() > 0.0);
                        auto request = whole->getVideo(range.start_time());
                        const VideoFrame frame = request.future.get();
                        FTK_CHECK(!frame.layers.empty());
                        FTK_CHECK(frame.layers[0].image);
                        FTK_CHECK(0 == whole->getReadErrorCount());
                    }

                    // Holding is not an error: the policy dealt with it.
                    options.ioOptions["SeqIO/MissingFrames"] =
                        to_string(MissingFrames::Hold);
                    auto held = Timeline::create(_context, seqPath, options);
                    auto heldRequest = held->getVideo(
                        OTIO_NS::RationalTime(3.0, 24.0));
                    const VideoFrame heldFrame = heldRequest.future.get();
                    FTK_CHECK(!heldFrame.layers.empty());
                    FTK_CHECK(heldFrame.layers[0].image);
                    FTK_CHECK(0 == held->getReadErrorCount());
                }
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }

            // Clips may carry a color, which the timeline widget uses to color
            // the clip. This checks that the color survives being read, since
            // the color is optional and clips without one keep the default.
            try
            {
                const ftk::Path path(TLRENDER_SAMPLE_DATA, "ClipColors.otio");
                _print(ftk::Format("Path: {0}").arg(path.get()));
                auto timeline = Timeline::create(_context, path);
                const auto otioClips =
                    timeline->getOTIOTimeline().value->find_children<OTIO_NS::Clip>();
                FTK_CHECK(2 == otioClips.size());

                const auto color = otioClips[0]->color();
                FTK_CHECK(color.has_value());
                FTK_CHECK(0.75 == color.value().r());
                FTK_CHECK(0.25 == color.value().g());
                FTK_CHECK(0.125 == color.value().b());
                FTK_CHECK(1.0 == color.value().a());

                FTK_CHECK(!otioClips[1]->color().has_value());
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }

            // Media frame numbers are only a coordinate on the timeline when
            // the timeline is one media played through. These are the two ways
            // that fails, and a caller showing a frame number has to count the
            // timeline instead for both of them.
            try
            {
                // Different media in each clip, so the numbering restarts at
                // every cut. The first two clips also run at a different rate
                // than the timeline, which repeats a number from one frame to
                // the next.
                auto cuts = Timeline::create(
                    _context,
                    ftk::Path(TLRENDER_SAMPLE_DATA, "MultipleClips.otio"));
                FTK_CHECK(!cuts->isMediaTimeContinuous());

                // One media, but taken twice, so the numbering goes backwards
                // at the join.
                auto repeated = Timeline::create(
                    _context,
                    ftk::Path(TLRENDER_SAMPLE_DATA, "RepeatClip.otio"));
                FTK_CHECK(!repeated->isMediaTimeContinuous());

                // One media in one piece, which is the ordinary case.
                auto single = Timeline::create(
                    _context,
                    ftk::Path(TLRENDER_SAMPLE_DATA, "SingleClip.otio"));
                FTK_CHECK(single->isMediaTimeContinuous());
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }
        }

        void TimelineTest::_synchronous()
        {
            // A timeline with no thread has to produce what a threaded one
            // produces; it is the same request path, run by the caller.
            const ftk::Path path(TLRENDER_SAMPLE_DATA, "SingleClipSeq.otioz");
            Options options;
            options.threaded = false;
            auto sync = Timeline::create(_context, path, options);
            auto threaded = Timeline::create(_context, path);
            FTK_CHECK(sync->getTimeRange() == threaded->getTimeRange());
            FTK_CHECK(sync->getIOInfo().video == threaded->getIOInfo().video);

            const size_t frameCount = std::min(
                static_cast<size_t>(sync->getTimeRange().duration().value()),
                size_t(5));
            for (size_t i = 0; i < frameCount; ++i)
            {
                const OTIO_NS::RationalTime time(i, 24.0);

                // The future is already resolved when there is no thread.
                auto request = sync->getVideo(time);
                FTK_CHECK(request.future.valid());
                FTK_CHECK(request.future.wait_for(std::chrono::seconds(0)) ==
                    std::future_status::ready);
                const VideoFrame a = request.future.get();

                auto other = threaded->getVideo(time);
                const VideoFrame b = other.future.get();

                FTK_CHECK(a.layers.size() == b.layers.size());
                for (size_t j = 0; j < a.layers.size(); ++j)
                {
                    const auto& imageA = a.layers[j].image;
                    const auto& imageB = b.layers[j].image;
                    FTK_CHECK(!imageA == !imageB);
                    if (imageA && imageB)
                    {
                        FTK_CHECK(imageA->getSize() == imageB->getSize());
                        FTK_CHECK(imageA->getByteCount() == imageB->getByteCount());
                        FTK_CHECK(0 == memcmp(
                            imageA->getData(),
                            imageB->getData(),
                            imageB->getByteCount()));
                    }
                }
            }
            _print("synchronous timeline matches the threaded one");
        }

        void TimelineTest::_media()
        {
            // Reading media named by path out of a bundle, which is what a
            // thumbnail needs: the frames are byte ranges, so opening the
            // media path directly would not find a file.
            const ftk::Path path(TLRENDER_SAMPLE_DATA, "SingleClipSeq.otioz");
            Options options;
            options.threaded = false;
            auto timeline = Timeline::create(_context, path, options);

            const auto mediaPaths = timeline->getMediaPaths();
            FTK_CHECK(!mediaPaths.empty());
            for (const auto& mediaPath : mediaPaths)
            {
                _print(ftk::Format("Media: {0}").arg(mediaPath.get()));

                IOInfo info;
                if (!timeline->getMediaInfo(mediaPath, info))
                {
                    // No plugin can read this media; a build without any
                    // image formats can read none of it.
                    continue;
                }
                if (info.video.empty())
                {
                    // A bundle carries its audio as media too.
                    continue;
                }

                auto future = timeline->readMedia(
                    mediaPath, info.videoTime->start_time());
                FTK_CHECK(future.valid());
                // No thread, so the read is already done.
                FTK_CHECK(future.wait_for(std::chrono::seconds(0)) ==
                    std::future_status::ready);
                const VideoData data = future.get();
                FTK_CHECK(data.image);
                FTK_CHECK(data.image->getSize() == info.video[0].size);
            }

            // Media that is not in the timeline.
            IOInfo info;
            FTK_CHECK(!timeline->getMediaInfo(
                ftk::Path("/nowhere/absent.exr"), info));
            FTK_CHECK(!timeline->readMedia(
                ftk::Path("/nowhere/absent.exr"),
                OTIO_NS::RationalTime(0.0, 24.0)).valid());

            // A plain file is a timeline of one clip, and its own path names
            // that clip's media. That is what lets a caller always ask by
            // (timeline, media) without knowing which it has.
            const ftk::Path seqPath(
                TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.0001.jpg");
            // Making a timeline out of a plain file needs a plugin that
            // reads it; a build with no image formats cannot, and asking
            // throws rather than returning nothing.
            const bool seqReadable =
                canRead(_context, "Seq/BART_2021-02-07.0001.jpg");
            if (seqReadable)
            {
                auto seqTimeline = Timeline::create(_context, seqPath, options);
                const auto seqMedia = seqTimeline->getMediaPaths();
                for (const auto& i : seqMedia)
                {
                    _print(ftk::Format("Seq media: {0}").arg(i.get()));
                }
                // More than one: the sequence, and the audio file found
                // beside it. What matters is that the file's own path names
                // its media.
                const auto j = std::find_if(
                    seqMedia.begin(),
                    seqMedia.end(),
                    [&seqPath](const ftk::Path& i)
                    {
                        return i.get() == seqPath.get();
                    });
                FTK_CHECK(j != seqMedia.end());
                // Only a build with an image format can read it, but
                // whichever build this is, saying it read something and
                // returning nothing would be wrong.
                IOInfo seqInfo;
                if (seqTimeline->getMediaInfo(seqPath, seqInfo))
                {
                    FTK_CHECK(!seqInfo.video.empty());
                }
            }

            // The decoding thread count is configurable, and is read where
            // the pool is started rather than being a constant.
            // The bundle rather than the image sequence: what is being
            // checked is how the options reach the pool, which does not
            // depend on any format being readable.
            {
                Options threadOptions;
                threadOptions.readThreadCount = 3;
                auto t = Timeline::create(_context, path, threadOptions);
                FTK_CHECK(3 == t->getReadThreadCount());
                auto d = Timeline::create(_context, path);
                FTK_CHECK(d->getReadThreadCount() == getDefaultReadThreadCount());

                // How many requests are in flight follows the thread count,
                // so asking for more decoding threads is not undone by a
                // separate limit that nothing mentions. This used to stop at
                // sixteen whatever the thread count said.
                FTK_CHECK(6 == t->getVideoRequestMax());
                Options manyOptions;
                manyOptions.readThreadCount = 24;
                auto many = Timeline::create(_context, path, manyOptions);
                FTK_CHECK(24 == many->getReadThreadCount());
                FTK_CHECK(many->getVideoRequestMax() > 16);

                // Zero threads must not mean zero requests in flight: that
                // would leave a timeline without a thread waiting for a
                // request nothing was ever going to pick up.
                Options zeroOptions;
                zeroOptions.threaded = false;
                zeroOptions.readThreadCount = 0;
                auto zero = Timeline::create(_context, path, zeroOptions);
                FTK_CHECK(zero->getVideoRequestMax() > 0);
                auto zeroRequest = zero->getVideo(
                    zero->getTimeRange().start_time());
                FTK_CHECK(zeroRequest.future.valid());
                FTK_CHECK(zeroRequest.future.wait_for(std::chrono::seconds(0)) ==
                    std::future_status::ready);
            }

            _print("named media read from a bundle");
        }
}
}

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

#include <cstring>
#include <ftk/Core/Format.h>

#include <opentimelineio/clip.h>
#include <opentimelineio/externalReference.h>
#include <opentimelineio/imageSequenceReference.h>
#include <opentimelineio/timeline.h>

#include <algorithm>

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
                        FTK_ASSERT(i.canvasSize == videoFrame.canvasSize);
                        FTK_ASSERT(!videoFrame.layers.empty());
                        FTK_ASSERT(videoFrame.layers[0].bounds.has_value());
                        FTK_ASSERT(frame.second == videoFrame.layers[0].bounds.value());
                    }
                }
                catch (const std::exception& e)
                {
                    _error(e.what());
                }
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
                FTK_ASSERT(!videoFrame.canvasSize.isValid());
                for (const auto& layer : videoFrame.layers)
                {
                    FTK_ASSERT(!layer.bounds.has_value());
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
                FTK_ASSERT(ftk::Size2I(1920, 1080) == videoFrame.canvasSize);
                FTK_ASSERT(!videoFrame.layers.empty());
                FTK_ASSERT(videoFrame.layers[0].bounds.has_value());
                FTK_ASSERT(full == videoFrame.layers[0].bounds.value());
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
                FTK_ASSERT(!videoFrame.canvasSize.isValid());
                for (const auto& layer : videoFrame.layers)
                {
                    FTK_ASSERT(!layer.bounds.has_value());
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
                    FTK_ASSERT(!videoFrame.layers.empty());
                    FTK_ASSERT(!videoFrame.layers[0].bounds.has_value());
                }
                {
                    Options options;
                    options.spatial = Spatial::Normalize;
                    auto timeline = Timeline::create(_context, path, options);
                    auto request = timeline->getVideo(OTIO_NS::RationalTime(30.0, 24.0));
                    const VideoFrame videoFrame = request.future.get();
                    FTK_ASSERT(ftk::Size2I(1920, 1080) == videoFrame.canvasSize);
                    FTK_ASSERT(!videoFrame.layers.empty());
                    FTK_ASSERT(videoFrame.layers[0].bounds.has_value());
                    FTK_ASSERT(full == videoFrame.layers[0].bounds.value());
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
            FTK_ASSERT(a == a);
            FTK_ASSERT(a != Options());

            // The cache sizes are options rather than constants so that a
            // caller can ask for what it needs; comparing them is what keeps
            // a timeline from being reused with somebody else's sizes.
            Options cache;
            cache.readCacheMax = 1;
            FTK_ASSERT(cache != Options());
            Options seq;
            seq.seqCacheMax = 1;
            FTK_ASSERT(seq != Options());
        }

        void TimelineTest::_util()
        {
        }

        void TimelineTest::_transitions()
        {
            {
                FTK_ASSERT(toTransition(std::string()) == Transition::None);
                FTK_ASSERT(toTransition("SMPTE_Dissolve") == Transition::Dissolve);
            }
        }

        void TimelineTest::_videoData()
        {
            {
                VideoLayer a, b;
                FTK_ASSERT(a == b);
                a.transition = Transition::Dissolve;
                FTK_ASSERT(a != b);
            }
            {
                VideoData a, b;
                FTK_ASSERT(a == b);
                a.time = OTIO_NS::RationalTime(1.0, 24.0);
                FTK_ASSERT(a != b);
            }
        }

        void TimelineTest::_timeline()
        {
            // Test timelines.
            const std::vector<ftk::Path> paths =
            {
                ftk::Path(TLRENDER_SAMPLE_DATA, "Seq/BART_2021-02-07.0001.jpg"),
#if defined(TLRENDER_FFMPEG_PLUGIN) || defined(TLRENDER_FFMPEG_CMD)
                ftk::Path(TLRENDER_SAMPLE_DATA, "BART_2021-02-07.m4v"),
                ftk::Path(TLRENDER_SAMPLE_DATA, "MovieAndSeq.otio"),
                ftk::Path(TLRENDER_SAMPLE_DATA, "TransitionGap.otio"),
#endif // TLRENDER_FFMPEG_PLUGIN or TLRENDER_FFMPEG_CMD
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
            FTK_ASSERT(videoRequests.empty());

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
            FTK_ASSERT(audioRequests.empty());

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
                (_getTempDir() / "TimelineSilent.mov").u8string());
            const ftk::Path audioPath(
                (_getTempDir() / "TimelineAudio.mov").u8string());
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
                    write->writeAudio(info.audioTime, samples);
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
                _error(ftk::Format(
                    "A movie with no audio should cost one reader, not {0}").
                    arg(silentCount));
                FTK_ASSERT(false);
            }
            const size_t audioCount = readerCount(audioPath);
            _print(ftk::Format("Movie with audio readers: {0}").arg(audioCount));
            if (audioCount != 2)
            {
                _error(ftk::Format(
                    "A movie with audio should cost two readers, not {0}").
                    arg(audioCount));
                FTK_ASSERT(false);
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
            const auto clips = timeline->getTimeline()->find_clips();
            FTK_ASSERT(!clips.empty());
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
            FTK_ASSERT(decode);

            auto mem = timeline->getMem(mediaReference);
            FTK_ASSERT(!mem.empty());
            auto seq = SeqDecode::create(mediaPath, mem, decode);
            const IOInfo info = seq->getInfo();
            FTK_ASSERT(!info.video.empty());
            const auto time = info.videoTime.start_time();
            FTK_ASSERT(seq->readVideo(time).image);

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
            FTK_ASSERT(data.image);
            FTK_ASSERT(data.image->getSize() == info.video[0].size);

            _print("a decoder outlives the timeline that opened it");
        }

        void TimelineTest::_separateAudio()
        {
#if defined(TLRENDER_FFMPEG_PLUGIN) || defined(TLRENDER_FFMPEG_CMD)
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
                FTK_ASSERT(audioPath.isEmpty());
                _print(ftk::Format("Audio path: {0}").arg(audioPath.get()));
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
                options.imageSeqAudio = ImageSeqAudio::Ext;
                auto timeline = Timeline::create(_context, path, options);
                const ftk::Path& audioPath = timeline->getAudioPath();
                FTK_ASSERT(!audioPath.isEmpty());
                _print(ftk::Format("Audio path: {0}").arg(audioPath.get()));
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
                options.imageSeqAudio = ImageSeqAudio::FileName;
                options.imageSeqAudioFileName = ftk::Path(
                    TLRENDER_SAMPLE_DATA, "AudioToneStereo.wav").get();
                auto timeline = Timeline::create(_context, path, options);
                const ftk::Path& audioPath = timeline->getAudioPath();
                FTK_ASSERT(!audioPath.isEmpty());
                _print(ftk::Format("Audio path: {0}").arg(audioPath.get()));
            }
            catch (const std::exception& e)
            {
                _error(e.what());
            }
#endif // TLRENDER_FFMPEGPLUGIN or TLRENDER_FFMPEG_CMD
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
                FTK_ASSERT(3 == keys.size());
                FTK_ASSERT(keys.end() != std::find(keys.begin(), keys.end(), "Full"));
                FTK_ASSERT(keys.end() != std::find(keys.begin(), keys.end(), "Proxy"));
                FTK_ASSERT(keys.end() != std::find(
                    keys.begin(), keys.end(), OTIO_NS::Clip::default_media_key));

                // Without a key the clips are read from the media reference
                // OTIO has active.
                FTK_ASSERT(timeline->getMediaReferenceKey().empty());
                std::optional<ftk::Box2F> bounds;
                {
                    auto request = timeline->getVideo(OTIO_NS::RationalTime(0.0, 24.0));
                    const VideoFrame videoFrame = request.future.get();
                    FTK_ASSERT(!videoFrame.layers.empty());
                    FTK_ASSERT(videoFrame.layers[0].image);
                    FTK_ASSERT(smallSize == videoFrame.layers[0].image->getSize());
                    // The proxy is being read, but the canvas is built for the
                    // full resolution reference that the clip can be switched
                    // to, so no resolution is lost by opening on the proxy.
                    FTK_ASSERT(canvasSize == videoFrame.canvasSize);
                    bounds = videoFrame.layers[0].bounds;
                    FTK_ASSERT(bounds.has_value());
                }

                // Switching to the full resolution reference changes the image
                // that is read. The canvas and the box the clip occupies
                // within it are unchanged, which is the point of the feature:
                // the resolution changes underneath a fixed layout.
                timeline->setMediaReferenceKey("Full");
                FTK_ASSERT("Full" == timeline->getMediaReferenceKey());
                // The reported information follows the media reference being
                // read, so that it describes what is on screen.
                FTK_ASSERT(!timeline->getIOInfo().video.empty());
                FTK_ASSERT(largeSize == timeline->getIOInfo().video[0].size);
                {
                    auto request = timeline->getVideo(OTIO_NS::RationalTime(0.0, 24.0));
                    const VideoFrame videoFrame = request.future.get();
                    FTK_ASSERT(!videoFrame.layers.empty());
                    FTK_ASSERT(videoFrame.layers[0].image);
                    FTK_ASSERT(largeSize == videoFrame.layers[0].image->getSize());
                    FTK_ASSERT(canvasSize == videoFrame.canvasSize);
                    FTK_ASSERT(bounds == videoFrame.layers[0].bounds);
                }

                // The second clip has no reference under this key, so it falls
                // back to the default media key and is left as it was.
                {
                    auto request = timeline->getVideo(OTIO_NS::RationalTime(30.0, 24.0));
                    const VideoFrame videoFrame = request.future.get();
                    FTK_ASSERT(!videoFrame.layers.empty());
                    FTK_ASSERT(videoFrame.layers[0].image);
                    FTK_ASSERT(smallSize == videoFrame.layers[0].image->getSize());
                    FTK_ASSERT(canvasSize == videoFrame.canvasSize);
                }

                // A key set for a single clip overrides the timeline wide key.
                const auto otioClips =
                    timeline->getTimeline().value->find_children<OTIO_NS::Clip>();
                FTK_ASSERT(2 == otioClips.size());
                timeline->setMediaReferenceKey(otioClips[0], "Proxy");
                FTK_ASSERT("Proxy" == timeline->getMediaReferenceKey(otioClips[0]));
                {
                    auto request = timeline->getVideo(OTIO_NS::RationalTime(0.0, 24.0));
                    const VideoFrame videoFrame = request.future.get();
                    FTK_ASSERT(!videoFrame.layers.empty());
                    FTK_ASSERT(videoFrame.layers[0].image);
                    FTK_ASSERT(smallSize == videoFrame.layers[0].image->getSize());
                    FTK_ASSERT(bounds == videoFrame.layers[0].bounds);
                }

                // Clearing the key for the clip returns it to the timeline
                // wide key.
                timeline->setMediaReferenceKey(otioClips[0], std::string());
                FTK_ASSERT(timeline->getMediaReferenceKey(otioClips[0]).empty());
                {
                    auto request = timeline->getVideo(OTIO_NS::RationalTime(0.0, 24.0));
                    const VideoFrame videoFrame = request.future.get();
                    FTK_ASSERT(!videoFrame.layers.empty());
                    FTK_ASSERT(videoFrame.layers[0].image);
                    FTK_ASSERT(largeSize == videoFrame.layers[0].image->getSize());
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
                    timeline->getTimeline().value->find_children<OTIO_NS::Clip>();
                FTK_ASSERT(2 == otioClips.size());

                // Both of the first clip's references are mapped, including
                // the one that is not active.
                const auto mediaReferences = otioClips[0]->media_references();
                FTK_ASSERT(2 == mediaReferences.size());
                for (const auto& i : mediaReferences)
                {
                    FTK_ASSERT(!timeline->getMem(i.second).empty());
                }

                // The bundle opens on the proxy and switches to the full
                // resolution reference without re-reading the file.
                {
                    auto request = timeline->getVideo(OTIO_NS::RationalTime(0.0, 24.0));
                    const VideoFrame videoFrame = request.future.get();
                    FTK_ASSERT(!videoFrame.layers.empty());
                    FTK_ASSERT(videoFrame.layers[0].image);
                    FTK_ASSERT(smallSize == videoFrame.layers[0].image->getSize());
                    FTK_ASSERT(canvasSize == videoFrame.canvasSize);
                }
                timeline->setMediaReferenceKey("Full");
                {
                    auto request = timeline->getVideo(OTIO_NS::RationalTime(0.0, 24.0));
                    const VideoFrame videoFrame = request.future.get();
                    FTK_ASSERT(!videoFrame.layers.empty());
                    FTK_ASSERT(videoFrame.layers[0].image);
                    FTK_ASSERT(largeSize == videoFrame.layers[0].image->getSize());
                    FTK_ASSERT(canvasSize == videoFrame.canvasSize);
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
                    timeline->getTimeline().value->find_children<OTIO_NS::Clip>();
                const auto mediaReferences = otioClips[0]->media_references();
                FTK_ASSERT(2 == mediaReferences.size());
                FTK_ASSERT(!timeline->getMem(mediaReferences.at("Proxy")).empty());
                FTK_ASSERT(timeline->getMem(mediaReferences.at("Full")).empty());

                // The active reference still reads.
                {
                    auto request = timeline->getVideo(OTIO_NS::RationalTime(0.0, 24.0));
                    const VideoFrame videoFrame = request.future.get();
                    FTK_ASSERT(!videoFrame.layers.empty());
                    FTK_ASSERT(videoFrame.layers[0].image);
                    FTK_ASSERT(smallSize == videoFrame.layers[0].image->getSize());
                }

                // The missing reference does not. Its path resolves to a file
                // that exists next to the sample data, so this would give an
                // image if the media were being taken from the file system.
                timeline->setMediaReferenceKey("Full");
                {
                    auto request = timeline->getVideo(OTIO_NS::RationalTime(0.0, 24.0));
                    const VideoFrame videoFrame = request.future.get();
                    FTK_ASSERT(!videoFrame.layers.empty());
                    FTK_ASSERT(!videoFrame.layers[0].image);
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
                    timeline->getTimeline().value->find_children<OTIO_NS::Clip>();
                FTK_ASSERT(2 == otioClips.size());

                const auto color = otioClips[0]->color();
                FTK_ASSERT(color.has_value());
                FTK_ASSERT(0.75 == color.value().r());
                FTK_ASSERT(0.25 == color.value().g());
                FTK_ASSERT(0.125 == color.value().b());
                FTK_ASSERT(1.0 == color.value().a());

                FTK_ASSERT(!otioClips[1]->color().has_value());
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
            FTK_ASSERT(sync->getTimeRange() == threaded->getTimeRange());
            FTK_ASSERT(sync->getIOInfo().video == threaded->getIOInfo().video);

            const size_t frameCount = std::min(
                static_cast<size_t>(sync->getTimeRange().duration().value()),
                size_t(5));
            for (size_t i = 0; i < frameCount; ++i)
            {
                const OTIO_NS::RationalTime time(i, 24.0);

                // The future is already resolved when there is no thread.
                auto request = sync->getVideo(time);
                FTK_ASSERT(request.future.valid());
                FTK_ASSERT(request.future.wait_for(std::chrono::seconds(0)) ==
                    std::future_status::ready);
                const VideoFrame a = request.future.get();

                auto other = threaded->getVideo(time);
                const VideoFrame b = other.future.get();

                FTK_ASSERT(a.layers.size() == b.layers.size());
                for (size_t j = 0; j < a.layers.size(); ++j)
                {
                    const auto& imageA = a.layers[j].image;
                    const auto& imageB = b.layers[j].image;
                    FTK_ASSERT(!imageA == !imageB);
                    if (imageA && imageB)
                    {
                        FTK_ASSERT(imageA->getSize() == imageB->getSize());
                        FTK_ASSERT(imageA->getByteCount() == imageB->getByteCount());
                        FTK_ASSERT(0 == memcmp(
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
            FTK_ASSERT(!mediaPaths.empty());
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
                    mediaPath, info.videoTime.start_time());
                FTK_ASSERT(future.valid());
                // No thread, so the read is already done.
                FTK_ASSERT(future.wait_for(std::chrono::seconds(0)) ==
                    std::future_status::ready);
                const VideoData data = future.get();
                FTK_ASSERT(data.image);
                FTK_ASSERT(data.image->getSize() == info.video[0].size);
            }

            // Media that is not in the timeline.
            IOInfo info;
            FTK_ASSERT(!timeline->getMediaInfo(
                ftk::Path("/nowhere/absent.exr"), info));
            FTK_ASSERT(!timeline->readMedia(
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
                FTK_ASSERT(j != seqMedia.end());
                // Only a build with an image format can read it, but
                // whichever build this is, saying it read something and
                // returning nothing would be wrong.
                IOInfo seqInfo;
                if (seqTimeline->getMediaInfo(seqPath, seqInfo))
                {
                    FTK_ASSERT(!seqInfo.video.empty());
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
                FTK_ASSERT(3 == t->getReadThreadCount());
                auto d = Timeline::create(_context, path);
                FTK_ASSERT(d->getReadThreadCount() == getDefaultReadThreadCount());

                // How many requests are in flight follows the thread count,
                // so asking for more decoding threads is not undone by a
                // separate limit that nothing mentions. This used to stop at
                // sixteen whatever the thread count said.
                FTK_ASSERT(6 == t->getVideoRequestMax());
                Options manyOptions;
                manyOptions.readThreadCount = 24;
                auto many = Timeline::create(_context, path, manyOptions);
                FTK_ASSERT(24 == many->getReadThreadCount());
                FTK_ASSERT(many->getVideoRequestMax() > 16);

                // Zero threads must not mean zero requests in flight: that
                // would leave a timeline without a thread waiting for a
                // request nothing was ever going to pick up.
                Options zeroOptions;
                zeroOptions.threaded = false;
                zeroOptions.readThreadCount = 0;
                auto zero = Timeline::create(_context, path, zeroOptions);
                FTK_ASSERT(zero->getVideoRequestMax() > 0);
                auto zeroRequest = zero->getVideo(
                    zero->getTimeRange().start_time());
                FTK_ASSERT(zeroRequest.future.valid());
                FTK_ASSERT(zeroRequest.future.wait_for(std::chrono::seconds(0)) ==
                    std::future_status::ready);
            }

            _print("named media read from a bundle");
        }
}
}

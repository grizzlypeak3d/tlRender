// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/C/Engine.h>
#include <tlRender/C/EngineCpp.h>

#include <tlRender/GPU/Render.h>
#include <tlRender/Timeline/CompareOptions.h>
#include <tlRender/Timeline/Init.h>
#include <tlRender/Timeline/OTIOVersion.h>
#include <tlRender/Timeline/Player.h>
#include <tlRender/Timeline/Timeline.h>
#include <tlRender/UI/Init.h>
#include <tlRender/UI/ItemOptions.h>
#include <tlRender/UI/ThumbnailSystem.h>
#include <tlRender/UI/TimelineWidget.h>

#include <ftk/UI/Event.h>
#include <ftk/UI/IWindow.h>
#include <ftk/UI/IconSystem.h>
#include <ftk/UI/Style.h>

#include <ftk/GPU/OffscreenBuffer.h>
#include <ftk/GPU/Present.h>
#include <ftk/GPU/Render.h>
#include <ftk/GPU/System.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/FontSystem.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/FileLogSystem.h>
#include <ftk/Core/LogSystem.h>
#include <ftk/Core/Matrix.h>
#include <ftk/Core/OS.h>
#include <ftk/Core/Path.h>
#include <ftk/Core/String.h>
#include <ftk/Core/ObservableList.h>

#include <SDL3/SDL.h>
// For SDL_SetMainReady() only: SDL did not start this program, and the
// main it would otherwise insist on is the application's own.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif // __APPLE__

#include <opentimelineio/clip.h>
#include <opentimelineio/externalReference.h>
#include <opentimelineio/marker.h>
#include <opentimelineio/stackAlgorithm.h>
#include <opentimelineio/timeline.h>
#include <opentimelineio/track.h>

#include <tlRender/Core/AudioResample.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <list>
#include <mutex>
#include <thread>
#include <cstdlib>
#include <cstring>
#include <future>
#include <map>
#include <string>
#include <vector>

struct tl_engine
{
    std::shared_ptr<ftk::Context> context;
    std::shared_ptr<ftk::FileLogSystem> fileLog;
    std::shared_ptr<ftk::gpu::System> gpu;
    std::string error;
    std::shared_ptr<ftk::ListObserver<ftk::LogItem> > logObserver;
    tl::IOOptions ioOptions;
    float videoGB = 0.F;
    int readCacheMax = 0;

    // Posters of timelines: asked for from any thread.
    std::mutex posterMutex;
    std::map<unsigned long long, std::future<std::shared_ptr<ftk::Image> > > posters;
};

struct tl_timeline
{
    tl_engine* engine = nullptr;
    std::shared_ptr<tl::Timeline> timeline;
    std::vector<tl_clip> clips;
    int videoTracks = 0;
};

namespace
{
    //! The file to open for a path: an .otiod bundle is a folder, and its
    //! timeline is the content.otio in it, the media beside that.
    std::string openPath(const char* path)
    {
        std::string out(path);
        while (out.size() > 1 && ('/' == out.back() || '\\' == out.back()))
        {
            out.pop_back();
        }
        if (ftk::toLower(ftk::Path(out).getExt()) == ".otiod")
        {
            out += "/content.otio";
        }
        return out;
    }

    //! A waveform to read: a range of the timeline in seconds from its
    //! start, and how many columns.
    struct WaveformJob
    {
        unsigned long long id = 0;
        double start = 0.0;
        double duration = 0.0;
        int columns = 0;
        std::atomic<bool> cancelled{ false };
        std::promise<std::vector<float> > promise;
    };
}

struct tl_player
{
    tl_engine* engine = nullptr;
    std::shared_ptr<tl::Timeline> timeline;
    std::shared_ptr<tl::Player> player;
    //! What the timeline is compared with, if anything.
    std::shared_ptr<tl::Timeline> compare;
    bool clipsRead = false;
    int videoTracks = 0;
    std::vector<tl_clip> clips;
    std::vector<tl_marker> markers;
    std::map<unsigned long long, std::future<std::shared_ptr<ftk::Image> > > thumbnails;

    // The waveforms: read one at a time on a thread of their own.
    std::mutex waveformMutex;
    std::condition_variable waveformCV;
    std::list<std::shared_ptr<WaveformJob> > waveformQueue;
    std::map<unsigned long long, std::shared_ptr<WaveformJob> > waveformJobs;
    std::map<unsigned long long, std::future<std::vector<float> > > waveforms;
    unsigned long long waveformId = 0;
    std::atomic<bool> waveformRunning{ false };
    std::thread waveformThread;

    ~tl_player()
    {
        if (waveformThread.joinable())
        {
            {
                std::unique_lock<std::mutex> lock(waveformMutex);
                waveformRunning = false;
                for (const auto& job : waveformJobs)
                {
                    job.second->cancelled = true;
                }
            }
            waveformCV.notify_one();
            waveformThread.join();
        }
        // The thumbnails not yet made are not wanted.
        if (!thumbnails.empty() && engine && engine->context)
        {
            if (auto system = engine->context->getSystem<tl::ui::ThumbnailSystem>())
            {
                std::vector<uint64_t> ids;
                for (const auto& i : thumbnails)
                {
                    ids.push_back(i.first);
                }
                system->cancelRequests(ids);
            }
        }
    }
};

namespace
{
    //! A feather-tk window with no window of its own: the widgets in it
    //! are drawn into the view's buffer after the picture, and get their
    //! events from the view. The window is the whole buffer, with no
    //! background, and the timeline widget is placed in it where the
    //! view says.
    class WidgetHost : public ftk::IWindow
    {
    protected:
        void _init(const std::shared_ptr<ftk::Context>& context)
        {
            IWindow::_init(context, nullptr, "tl_view");
            setBackgroundRole(ftk::ColorRole::None);
            _style = ftk::Style::create(context);
            _fontSystem = context->getSystem<ftk::FontSystem>();
            _iconSystem = context->getSystem<ftk::IconSystem>();
            _timeline = tl::ui::TimelineWidget::create(context, shared_from_this());
            _timeline->setFrameView(true);
#if defined(__APPLE__) && TARGET_OS_IPHONE
            // A finger scrolls the timeline and the scroll bars only got
            // in the way: they showed for a frame on every resize, when
            // the frame view had not yet caught up with the new width.
            _timeline->setScrollBarsVisible(false);
#endif // TARGET_OS_IPHONE
            _timeline->setVisible(false);
        }

    public:
        static std::shared_ptr<WidgetHost> create(const std::shared_ptr<ftk::Context>& context)
        {
            auto out = std::shared_ptr<WidgetHost>(new WidgetHost);
            out->_init(context);
            return out;
        }

        uint32_t getID() const override { return 0; }
        int getScreen() const override { return 0; }

        void setMinimized(bool value)
        {
            auto options = _timeline->getDisplayOptions();
            if (value != options.minimize)
            {
                options.minimize = value;
                _timeline->setDisplayOptions(options);
            }
        }

        ftk::Size2I getTimelineSizeHint() const
        {
            return _timeline->getSizeHint();
        }

        //! Two fingers, as the window takes them; see tl_view_gesture().
        void gesture(const ftk::V2I& pos, const ftk::V2F& pan, float zoom)
        {
            _gesture(pos, pan, zoom);
        }

        void gestureEnd()
        {
            _gestureEnd();
        }

        void setPlayer(const std::shared_ptr<tl::Player>& player)
        {
            _timeline->setPlayer(player);
        }

        //! The buffer the window is drawn into, and where in it the
        //! timeline widget goes.
        void setFrames(const ftk::Size2I& bufferSize, const ftk::Box2I& timelineFrame, float displayScale)
        {
            if (bufferSize != getBufferSize())
            {
                _setSize(bufferSize, bufferSize);
            }
            setDisplayScale(displayScale);
            if (timelineFrame != _timelineFrame)
            {
                _timelineFrame = timelineFrame;
                _timeline->setVisible(_timelineFrame.isValid());
                setSizeUpdate();
            }
        }

        void setGeometry(const ftk::Box2I& value) override
        {
            IWindow::setGeometry(value);
            _timeline->setGeometry(_timelineFrame);
        }

        void tick()
        {
            ftk::TickEvent event;
            _tickRecursive(shared_from_this(), true, true, event);
        }

        bool needsDraw()
        {
            return
                _hasDrawUpdate(shared_from_this()) ||
                _hasSizeUpdate(shared_from_this());
        }

        //! Lay the widgets out and draw them; the render is between its
        //! begin and end, with the buffer's transform.
        void draw(const std::shared_ptr<ftk::IRender>& render)
        {
            IWindow::_update(_fontSystem, _iconSystem, _style);
            const ftk::Box2I drawRect(ftk::V2I(), getBufferSize());
            render->setClipRectEnabled(false);
            render->setClipRect(drawRect);
            ftk::DrawEvent event(_fontSystem, _iconSystem, getDisplayScale(), _style, render);
            _drawEventRecursive(shared_from_this(), drawRect, event);
            render->setClipRectEnabled(false);
        }

    private:
        void _tickRecursive(
            const std::shared_ptr<ftk::IWidget>& widget,
            bool visible,
            bool enabled,
            const ftk::TickEvent& event)
        {
            const bool parentsVisible = visible && widget->isVisible(false);
            const bool parentsEnabled = enabled && widget->isEnabled(false);
            for (const auto& child : widget->getChildren())
            {
                _tickRecursive(child, parentsVisible, parentsEnabled, event);
            }
            widget->tickEvent(visible, enabled, event);
        }

        std::shared_ptr<ftk::Style> _style;
        std::shared_ptr<ftk::FontSystem> _fontSystem;
        std::shared_ptr<ftk::IconSystem> _iconSystem;
        std::shared_ptr<tl::ui::TimelineWidget> _timeline;
        ftk::Box2I _timelineFrame;
    };
}

struct tl_view
{
    tl_engine* engine = nullptr;
    SDL_Window* window = nullptr;
    bool claimed = false;
    ftk::gpu::Composition composition = ftk::gpu::Composition::SDR;
    std::shared_ptr<ftk::gpu::Present> present;
    std::shared_ptr<tl::gpu::Render> render;
    std::shared_ptr<ftk::gpu::OffscreenBuffer> buffer;
    tl_player* player = nullptr;
    //! Where the picture goes, in pixels; empty for the whole view.
    ftk::Box2I frame;
    //! Where the picture is within it: fitted, or where it was put.
    bool viewFramed = true;
    ftk::V2F viewPos;
    float viewZoom = 1.F;
    //! What the last draw used, for a gesture to go on from.
    ftk::V2F viewPosNow;
    float viewZoomNow = 1.F;
    //! The widgets drawn over it, and where the timeline widget goes.
    std::shared_ptr<WidgetHost> widgets;
    ftk::Box2I timelineFrame;
    float displayScale = 1.F;
    //! What the picture is drawn with; see EngineCpp.h.
    tl::OCIOOptions ocioOptions;
    std::function<std::string(const std::string&, const ftk::ImageTags&)> ocioInputResolver;
    tl::LUTOptions lutOptions;
    ftk::ImageOptions imageOptions;
    tl::DisplayOptions displayOptions;
    tl::BackgroundOptions backgroundOptions;
    tl::ForegroundOptions foregroundOptions;
    tl::CompareOptions compareOptions;
    std::function<void(const std::shared_ptr<ftk::IRender>&)> overlay;
    //! Where the last draw put the picture; see EngineCpp.h.
    tl::c::ViewPicture picture;
    //! What the last draw showed, to know when there is something new:
    //! the frames (their time and image), the size, the frames' places.
    std::vector<std::pair<double, const void*> > drawnFrames;
    ftk::Size2I drawnSize;
    bool dirty = true;
    size_t drawCount = 0;
};

namespace
{
    void setError(tl_engine* engine, const std::string& what)
    {
        engine->error = what;
        if (engine->context)
        {
            engine->context->log("tl_engine", what, ftk::LogType::Error);
        }
    }

    //! How a timeline is read: by the GPU where it can, and as the
    //! engine's options say.
    tl::Options getTimelineOptions(tl_engine* engine)
    {
        tl::Options timelineOptions;
        // The GPU decodes what it can, which on a phone or a tablet is
        // the difference between playing and not; the reader falls back
        // to software for the rest.
        timelineOptions.ioOptions["FFmpeg/HWAccel"] = "1";
#if defined(__APPLE__) && TARGET_OS_IOS
        // The decoded frames stay on the GPU; see the "Texture" option in
        // IO.h. Only where SDL has the pixel buffer import (the super
        // build's patch), which the desktop's SDL may not.
        timelineOptions.ioOptions["Texture"] = "1";
        // Two frames read ahead in each reader rather than four: a 4K
        // frame is twelve megabytes, there are as many readers as clips
        // played lately, and the player's own cache is the one that
        // matters.
        timelineOptions.ioOptions["FFmpeg/VideoBufferSize"] = "2";
#endif // TARGET_OS_IOS
        for (const auto& i : engine->ioOptions)
        {
            timelineOptions.ioOptions[i.first] = i.second;
        }
        // Not fewer readers kept open, though each hardware decoder's
        // frame pool keeps every buffer it ever handed out, and with the
        // frames drawn where the decoder put them an idle reader's pool
        // grows as large as the cache: two readers froze the audio clock
        // on a timeline of three clips (a deadlock in the eviction, still
        // to be found), and the player's cache is sized for the pools.
        if (engine->readCacheMax > 0)
        {
            timelineOptions.readCacheMax = engine->readCacheMax;
        }
        return timelineOptions;
    }
}

extern "C"
{

tl_engine* tl_engine_create(const char* logFile)
{
    auto engine = new tl_engine;
    try
    {
        // SDL did not start this program, which on iOS it checks for:
        // said before anything initializes SDL, which tl::init does for
        // audio.
        SDL_SetMainReady();
        engine->context = ftk::Context::create();
        // The file before anything is logged, as ftk::App does it: the
        // previous run's moved aside, not being able to keep it no reason
        // not to start.
        if (logFile && *logFile)
        {
            const std::filesystem::path path = ftk::toFileSystem(logFile);
            std::error_code ec;
            std::filesystem::create_directories(path.parent_path(), ec);
            if (std::filesystem::exists(path, ec))
            {
                std::filesystem::path previous = path;
                previous.replace_extension("prev.log");
                std::filesystem::rename(path, previous, ec);
            }
            engine->fileLog = ftk::FileLogSystem::create(engine->context, path);
        }
        // The widgets as well as the engine: the timeline widget draws
        // in the view.
        tl::ui::init(engine->context);
        // The log goes to stderr, where Xcode and the simulator show it:
        // a failure below says why there, not just that it failed.
        engine->logObserver = ftk::ListObserver<ftk::LogItem>::create(
            engine->context->getSystem<ftk::LogSystem>()->observeLogItems(),
            [](const std::vector<ftk::LogItem>& items)
            {
                for (const auto& item : items)
                {
                    std::fprintf(stderr, "%s\n", ftk::getLabel(item).c_str());
                }
            });
        // The GPU renderer is the only one here: there is no OpenGL on
        // iOS, and a Swift view is a Metal layer.
        if (!ftk::gpu::start(engine->context))
        {
            throw std::runtime_error("The GPU renderer cannot start");
        }
        engine->gpu = engine->context->getSystem<ftk::gpu::System>();
    }
    catch (const std::exception& e)
    {
        setError(engine, e.what());
        // Kept, so the caller can read the error. Without the renderer
        // files still open and read; only the views need it.
        engine->gpu.reset();
    }
    return engine;
}

void tl_engine_set_io_option(tl_engine* engine, const char* name, const char* value)
{
    if (engine && name && value)
    {
        engine->ioOptions[name] = value;
    }
}

void tl_engine_set_cache(tl_engine* engine, float videoGB, int readCacheMax)
{
    if (engine)
    {
        engine->videoGB = videoGB;
        engine->readCacheMax = readCacheMax;
    }
}

void tl_engine_destroy(tl_engine* engine)
{
    delete engine;
}

void tl_engine_tick(tl_engine* engine)
{
    if (engine && engine->context)
    {
        engine->context->tick();
    }
}

int tl_engine_has_gpu(tl_engine* engine)
{
    return engine && engine->gpu ? 1 : 0;
}

const char* tl_engine_get_error(tl_engine* engine)
{
    return engine ? engine->error.c_str() : "";
}

tl_player* tl_player_create(tl_engine* engine, const char* path)
{
    if (!engine || !engine->context || !path)
        return nullptr;
    auto out = new tl_player;
    out->engine = engine;
    try
    {
        const tl::Options timelineOptions = getTimelineOptions(engine);
        out->timeline = tl::Timeline::create(engine->context, ftk::Path(openPath(path)), timelineOptions);
        tl::PlayerOptions options;
        // A second of cache before the clock runs: the picture moves
        // when it does, rather than standing while the playhead runs
        // ahead of the reader.
        options.preroll = 1.F;
#if defined(__APPLE__) && TARGET_OS_IOS
        // The desktop cache default is as big as an iPad's whole memory,
        // and iOS kills an application that takes much of it. An eighth
        // of the memory for video: the decoders' pools above can hold as
        // much again, and the whole must stay under the limit, about
        // three quarters of the memory. Audio in proportion.
        const size_t ram = ftk::getSysInfo().ram;
        if (ram > 0)
        {
            const float ramGB = ram / float(1024 * 1024 * 1024);
            options.cache.videoGB = ramGB / 8.F;
            options.cache.audioGB = ramGB / 32.F;
        }
#endif // TARGET_OS_IOS
        if (engine->videoGB > 0.F)
        {
            options.cache.videoGB = engine->videoGB;
        }
        out->player = tl::Player::create(engine->context, out->timeline, options);
    }
    catch (const std::exception& e)
    {
        setError(engine, e.what());
        delete out;
        out = nullptr;
    }
    return out;
}

void tl_player_destroy(tl_player* player)
{
    delete player;
}

int tl_player_set_compare(tl_player* player, const char* path)
{
    if (!player)
        return 0;
    std::shared_ptr<tl::Timeline> compare;
    if (path && path[0])
    {
        try
        {
            compare = tl::Timeline::create(
                player->engine->context,
                ftk::Path(openPath(path)),
                getTimelineOptions(player->engine));
        }
        catch (const std::exception& e)
        {
            setError(player->engine, e.what());
            return 0;
        }
    }
    player->compare = compare;
    player->player->setCompare(
        compare ?
        std::vector<std::shared_ptr<tl::Timeline> >{ compare } :
        std::vector<std::shared_ptr<tl::Timeline> >());
    return 1;
}

double tl_player_get_duration(tl_player* player)
{
    if (!player)
        return 0.0;
    const auto& range = player->player->getTimeRange();
    return range.duration().rescaled_to(1.0).value();
}

double tl_player_get_rate(tl_player* player)
{
    return player ? player->player->getTimeRange().duration().rate() : 0.0;
}

double tl_player_get_time(tl_player* player)
{
    if (!player)
        return 0.0;
    const auto& range = player->player->getTimeRange();
    const auto time = player->player->observeCurrentTime()->get();
    return (time - range.start_time()).rescaled_to(1.0).value();
}

void tl_player_seek(tl_player* player, double seconds)
{
    if (!player)
        return;
    const auto& range = player->player->getTimeRange();
    const double rate = range.duration().rate();
    player->player->seek(
        range.start_time() +
        OTIO_NS::RationalTime(seconds * rate, rate).round());
}

void tl_player_set_playback(tl_player* player, tl_playback value)
{
    if (!player)
        return;
    tl::Playback playback = tl::Playback::Stop;
    switch (value)
    {
    case TL_PLAYBACK_FORWARD: playback = tl::Playback::Forward; break;
    case TL_PLAYBACK_REVERSE: playback = tl::Playback::Reverse; break;
    default: break;
    }
    player->player->setPlayback(playback);
}

double tl_player_get_video_time(tl_player* player)
{
    if (!player)
        return -1.0;
    const auto frames = player->player->observeCurrentVideo()->get();
    if (frames.empty() || frames[0].layers.empty() || !frames[0].layers[0].image)
        return -1.0;
    const auto& range = player->player->getTimeRange();
    return (frames[0].time - range.start_time()).rescaled_to(1.0).value();
}

tl_player_stats tl_player_get_stats(tl_player* player)
{
    tl_player_stats out = {};
    if (!player)
        return out;
    const auto& p = player->player;
    out.speed = p->getSpeed();
    out.actualSpeed = p->getActualSpeed();
    out.droppedFrames = p->getDroppedFrames();
    out.buffering = p->isBuffering() ? 1 : 0;
    const auto time = p->observeCurrentTime()->get();
    const auto info = p->observeCacheInfo()->get();
    for (const auto& range : info.video)
    {
        if (range.contains(time))
        {
            out.cacheAhead = (range.end_time_exclusive() - time).rescaled_to(1.0).value();
            break;
        }
    }
    const auto& options = p->getCacheOptions();
    out.videoCachePercent = info.videoPercentage;
    out.videoCacheGB = options.videoGB;
    out.audioCachePercent = info.audioPercentage;
    out.audioCacheGB = options.audioGB;
    return out;
}

tl_playback tl_player_get_playback(tl_player* player)
{
    if (!player)
        return TL_PLAYBACK_STOP;
    switch (player->player->observePlayback()->get())
    {
    case tl::Playback::Forward: return TL_PLAYBACK_FORWARD;
    case tl::Playback::Reverse: return TL_PLAYBACK_REVERSE;
    default: return TL_PLAYBACK_STOP;
    }
}

void tl_player_set_loop(tl_player* player, tl_loop value)
{
    if (!player)
        return;
    tl::Loop loop = tl::Loop::Loop;
    switch (value)
    {
    case TL_LOOP_ONCE: loop = tl::Loop::Once; break;
    case TL_LOOP_PING_PONG: loop = tl::Loop::PingPong; break;
    default: break;
    }
    player->player->setLoop(loop);
}

tl_loop tl_player_get_loop(tl_player* player)
{
    if (!player)
        return TL_LOOP_LOOP;
    switch (player->player->getLoop())
    {
    case tl::Loop::Once: return TL_LOOP_ONCE;
    case tl::Loop::PingPong: return TL_LOOP_PING_PONG;
    default: break;
    }
    return TL_LOOP_LOOP;
}

double tl_player_get_speed(tl_player* player)
{
    return player ? player->player->getSpeed() : 0.0;
}

void tl_player_set_speed(tl_player* player, double value)
{
    if (!player)
        return;
    player->player->setSpeed(value);
}

void tl_player_time_action(tl_player* player, tl_time_action value)
{
    if (!player)
        return;
    tl::TimeAction action = tl::TimeAction::Start;
    switch (value)
    {
    case TL_TIME_END: action = tl::TimeAction::End; break;
    case TL_TIME_FRAME_PREV: action = tl::TimeAction::FramePrev; break;
    case TL_TIME_FRAME_PREV_X10: action = tl::TimeAction::FramePrevX10; break;
    case TL_TIME_FRAME_PREV_X100: action = tl::TimeAction::FramePrevX100; break;
    case TL_TIME_FRAME_NEXT: action = tl::TimeAction::FrameNext; break;
    case TL_TIME_FRAME_NEXT_X10: action = tl::TimeAction::FrameNextX10; break;
    case TL_TIME_FRAME_NEXT_X100: action = tl::TimeAction::FrameNextX100; break;
    case TL_TIME_JUMP_BACK_1S: action = tl::TimeAction::JumpBack1s; break;
    case TL_TIME_JUMP_BACK_10S: action = tl::TimeAction::JumpBack10s; break;
    case TL_TIME_JUMP_FORWARD_1S: action = tl::TimeAction::JumpForward1s; break;
    case TL_TIME_JUMP_FORWARD_10S: action = tl::TimeAction::JumpForward10s; break;
    default: break;
    }
    player->player->timeAction(action);
}

void tl_player_set_in_point(tl_player* player)
{
    if (player)
        player->player->setInPoint();
}

void tl_player_reset_in_point(tl_player* player)
{
    if (player)
        player->player->resetInPoint();
}

void tl_player_set_out_point(tl_player* player)
{
    if (player)
        player->player->setOutPoint();
}

void tl_player_reset_out_point(tl_player* player)
{
    if (player)
        player->player->resetOutPoint();
}

void tl_player_get_in_out(tl_player* player, double* start, double* end)
{
    double s = 0.0;
    double e = 0.0;
    if (player)
    {
        const auto& range = player->player->getTimeRange();
        const auto& inOut = player->player->getInOutRange();
        s = (inOut.start_time() - range.start_time()).rescaled_to(1.0).value();
        e = (inOut.end_time_exclusive() - range.start_time()).rescaled_to(1.0).value();
    }
    if (start)
        *start = s;
    if (end)
        *end = e;
}

void tl_player_set_in_out(tl_player* player, double start, double end)
{
    if (!player)
        return;
    const auto& range = player->player->getTimeRange();
    const double rate = range.duration().rate();
    const auto s = OTIO_NS::RationalTime(start * rate, rate).round();
    const auto e = OTIO_NS::RationalTime(end * rate, rate).round();
    player->player->setInOutRange(OTIO_NS::TimeRange::range_from_start_end_time(
        range.start_time() + s,
        range.start_time() + e));
}

namespace
{
    //! The key, in the metadata of a copy of a clip, of its index in the
    //! clip list: flattening copies clips, and the copies say which clip
    //! they came from by it.
    const std::string clipIndexKey = "tlC_clip";

    //! A clip or a nested stack: what plays on a track, gaps aside.
    bool isShown(const OTIO_NS::Composable* value)
    {
        return dynamic_cast<const OTIO_NS::Clip*>(value) ||
            dynamic_cast<const OTIO_NS::Stack*>(value);
    }

    //! The markers of the timeline, its enabled tracks and their clips,
    //! in seconds from the start of the timeline, in time order.
    void listMarkers(
        const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline>& timeline,
        std::vector<tl_marker>& out)
    {
        const auto stack = timeline->tracks();
        if (!stack)
            return;
        const double duration = timeline->duration().rescaled_to(1.0).value();
        const double globalStart = timeline->global_start_time().has_value() ?
            timeline->global_start_time()->rescaled_to(1.0).value() :
            0.0;
        const auto add = [&out, duration, globalStart](
            const OTIO_NS::Marker* marker,
            const OTIO_NS::TimeRange& range,
            bool global)
        {
            tl_marker m = {};
            m.start = range.start_time().rescaled_to(1.0).value();
            m.duration = range.duration().rescaled_to(1.0).value();
            // A timeline or track marker written in the timeline's own
            // clock, which starts at its global start time.
            if (global && globalStart > 0.0 && m.start >= duration &&
                m.start - globalStart >= 0.0 && m.start - globalStart <= duration)
            {
                m.start -= globalStart;
            }
            const ftk::Color4F color = tl::ui::getMarkerColor(marker->color());
            m.color[0] = color.r;
            m.color[1] = color.g;
            m.color[2] = color.b;
            m.color[3] = color.a;
            std::snprintf(m.name, sizeof(m.name), "%s", marker->name().c_str());
            std::snprintf(m.comment, sizeof(m.comment), "%s", marker->comment().c_str());
            out.push_back(m);
        };
        for (const auto& marker : stack->markers())
        {
            add(marker, marker->marked_range(), true);
        }
        for (const auto& child : stack->children())
        {
            auto track = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Track>(child);
            if (!track || !track->enabled())
                continue;
            for (const auto& marker : track->markers())
            {
                add(marker, marker->marked_range(), true);
            }
            for (const auto& trackChild : track->children())
            {
                auto item = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Item>(trackChild);
                if (!item || !isShown(item))
                    continue;
                for (const auto& marker : item->markers())
                {
                    // In the clip's own time: moved to the timeline's.
                    OTIO_NS::ErrorStatus error;
                    const auto range = item->transformed_time_range(
                        marker->marked_range(), stack, &error);
                    if (!OTIO_NS::is_error(error))
                    {
                        add(marker, range, false);
                    }
                }
            }
        }
        std::stable_sort(
            out.begin(),
            out.end(),
            [](const tl_marker& a, const tl_marker& b) { return a.start < b.start; });
    }

    //! The clips as seen: the video tracks flattened, as OTIO flattens
    //! them, the highest track's clip winning wherever tracks overlap. A
    //! clip that a higher track's cuts into is seen more than once.
    void listClips(
        const std::shared_ptr<tl::Timeline>& timeline,
        std::vector<tl_clip>& clips,
        int& videoTracks,
        std::vector<tl_marker>* markers)
    {
        // Every clip of every video track, which the flattened ones are
        // found in by their index.
        std::vector<tl_clip> sources;
        const auto& otioTimeline = timeline->getOTIOTimeline();
        if (!otioTimeline)
            return;

        // The tracks' clips, and a copy of the tracks with each clip
        // tagged with its index, to flatten.
        std::vector<OTIO_NS::SerializableObject::Retainer<OTIO_NS::Track> > copies;
        std::vector<OTIO_NS::Track*> tracks;
        int trackIndex = 0;
        for (const auto& track : otioTimeline->video_tracks())
        {
            OTIO_NS::SerializableObject::Retainer<OTIO_NS::Track> copy(
                dynamic_cast<OTIO_NS::Track*>(track->clone()));
            const auto& children = track->children();
            const auto& copyChildren = copy ? copy->children() : children;
            for (size_t i = 0; i < children.size(); ++i)
            {
                const auto& child = children[i];
                if (!isShown(child))
                    continue;
                auto item = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Item>(child);
                const auto range = item->trimmed_range_in_parent();
                if (!range.has_value())
                    continue;
                tl_clip out = {};
                out.track = trackIndex;
                out.start = range->start_time().rescaled_to(1.0).value();
                out.duration = range->duration().rescaled_to(1.0).value();
                std::string name = item->name();
                if (name.empty())
                {
                    if (auto clip = dynamic_cast<OTIO_NS::Clip*>(item.value))
                    {
                        if (auto ref = dynamic_cast<OTIO_NS::ExternalReference*>(
                            clip->media_reference()))
                        {
                            name = ftk::Path(ref->target_url()).getFileName();
                        }
                    }
                }
                std::snprintf(out.name, sizeof(out.name), "%s", name.c_str());
#if TLRENDER_OTIO_ITEM_COLOR
                if (const auto color = item->color())
                {
                    out.color[0] = static_cast<float>(color->r());
                    out.color[1] = static_cast<float>(color->g());
                    out.color[2] = static_cast<float>(color->b());
                    out.color[3] = static_cast<float>(color->a());
                    out.hasColor = 1;
                }
#endif // TLRENDER_OTIO_ITEM_COLOR
                if (copy && i < copyChildren.size())
                {
                    if (auto copyItem = dynamic_cast<OTIO_NS::Item*>(copyChildren[i].value))
                    {
                        copyItem->metadata()[clipIndexKey] =
                            static_cast<int64_t>(sources.size());
                    }
                }
                sources.push_back(out);
            }
            if (copy && copy->enabled())
            {
                tracks.push_back(copy);
                copies.push_back(copy);
            }
            ++trackIndex;
        }
        videoTracks = trackIndex;
        if (markers)
        {
            listMarkers(otioTimeline, *markers);
        }

        // The flattened track's clips, by the clip each came from. Pieces
        // of one clip that touch are one.
        if (tracks.empty())
            return;
        OTIO_NS::ErrorStatus error;
        OTIO_NS::SerializableObject::Retainer<OTIO_NS::Track> flat(
            OTIO_NS::flatten_stack(tracks, &error));
        if (!flat || OTIO_NS::is_error(error))
            return;
        std::vector<int64_t> indexes;
        for (const auto& child : flat->children())
        {
            if (!isShown(child))
                continue;
            auto item = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Item>(child);
            const auto i = item->metadata().find(clipIndexKey);
            if (i == item->metadata().end() || !i->second.has_value())
                continue;
            int64_t index = -1;
            try
            {
                index = std::any_cast<int64_t>(i->second);
            }
            catch (const std::bad_any_cast&)
            {
                continue;
            }
            if (index < 0 || index >= static_cast<int64_t>(sources.size()))
                continue;
            const auto range = item->trimmed_range_in_parent();
            if (!range.has_value() || range->duration().value() <= 0.0)
                continue;
            const double start = range->start_time().rescaled_to(1.0).value();
            const double duration = range->duration().rescaled_to(1.0).value();
            if (!clips.empty() &&
                indexes.back() == index &&
                std::abs(clips.back().start + clips.back().duration - start) < 1e-6)
            {
                clips.back().duration += duration;
                continue;
            }
            tl_clip clip = sources[index];
            clip.start = start;
            clip.duration = duration;
            clip.continued = std::find(indexes.begin(), indexes.end(), index) != indexes.end();
            clips.push_back(clip);
            indexes.push_back(index);
        }
    }

    void readClips(tl_player* player)
    {
        if (player->clipsRead)
            return;
        player->clipsRead = true;
        listClips(player->timeline, player->clips, player->videoTracks, &player->markers);
    }
}

int tl_player_get_video_track_count(tl_player* player)
{
    if (!player)
        return 0;
    readClips(player);
    return player->videoTracks;
}

int tl_player_get_clip_count(tl_player* player)
{
    if (!player)
        return 0;
    readClips(player);
    return static_cast<int>(player->clips.size());
}

int tl_player_get_clip(tl_player* player, int index, tl_clip* out)
{
    if (!player || !out)
        return 0;
    readClips(player);
    if (index < 0 || index >= static_cast<int>(player->clips.size()))
        return 0;
    *out = player->clips[index];
    return 1;
}


int tl_player_get_marker_count(tl_player* player)
{
    if (!player)
        return 0;
    readClips(player);
    return static_cast<int>(player->markers.size());
}

int tl_player_get_marker(tl_player* player, int index, tl_marker* out)
{
    if (!player || !out)
        return 0;
    readClips(player);
    if (index < 0 || index >= static_cast<int>(player->markers.size()))
        return 0;
    *out = player->markers[index];
    return 1;
}

void tl_image_free(tl_image* image)
{
    if (image)
    {
        std::free(image->data);
        image->data = nullptr;
        image->width = 0;
        image->height = 0;
    }
}

namespace
{
    //! A thumbnail into a picture for the caller: RGBA, the top row
    //! first.
    bool copyImage(const std::shared_ptr<ftk::Image>& image, tl_image* out)
    {
        if (!image || image->getInfo().type != ftk::ImageType::RGBA_U8)
            return false;
        const auto& info = image->getInfo();
        const size_t rowBytes = static_cast<size_t>(info.size.w) * 4;
        out->width = info.size.w;
        out->height = info.size.h;
        out->data = static_cast<unsigned char*>(std::malloc(rowBytes * info.size.h));
        if (!out->data)
            return false;
        // Rows can be padded to the image's alignment, and are stored
        // from the bottom up, as OpenGL has them, unless mirrored.
        const size_t stride = info.getByteCount() / info.size.h;
        for (int y = 0; y < info.size.h; ++y)
        {
            const int src = info.layout.mirror.y ? y : info.size.h - 1 - y;
            std::memcpy(out->data + y * rowBytes, image->getData() + src * stride, rowBytes);
        }
        return true;
    }
}

unsigned long long tl_player_request_thumbnail(tl_player* player, double seconds, int height)
{
    if (!player || height <= 0)
        return 0;
    auto thumbnails = player->engine->context->getSystem<tl::ui::ThumbnailSystem>();
    if (!thumbnails)
        return 0;
    const auto& range = player->player->getTimeRange();
    const double rate = range.duration().rate();
    auto request = thumbnails->getThumbnail(
        player->timeline->getPath(),
        height,
        range.start_time() + OTIO_NS::RationalTime(seconds * rate, rate).round(),
        player->timeline->getOptions().ioOptions);
    const unsigned long long id = request.id;
    player->thumbnails[id] = std::move(request.future);
    return id;
}

int tl_player_get_thumbnail(tl_player* player, unsigned long long id, tl_image* out)
{
    if (!player || !out)
        return -1;
    const auto i = player->thumbnails.find(id);
    if (i == player->thumbnails.end())
        return -1;
    if (!i->second.valid())
    {
        player->thumbnails.erase(i);
        return -1;
    }
    if (i->second.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return 0;
    std::shared_ptr<ftk::Image> image;
    try
    {
        image = i->second.get();
    }
    catch (const std::exception&)
    {}
    player->thumbnails.erase(i);
    return copyImage(image, out) ? 1 : -1;
}

void tl_player_cancel_thumbnail(tl_player* player, unsigned long long id)
{
    if (!player)
        return;
    const auto i = player->thumbnails.find(id);
    if (i == player->thumbnails.end())
        return;
    player->thumbnails.erase(i);
    if (auto thumbnails = player->engine->context->getSystem<tl::ui::ThumbnailSystem>())
    {
        thumbnails->cancelRequests({ id });
    }
}

// C++ linkage: this part of the file is inside the C interface's
// extern "C".
extern "C++"
{
namespace
{
    //! Read one waveform: the timeline's audio a second at a time, every
    //! layer, each sample counted in the column its time falls in. Lowest
    //! and highest a column, in pairs.
    std::vector<float> readWaveform(
        const std::shared_ptr<tl::Timeline>& timeline,
        const WaveformJob& job)
    {
        std::vector<float> out(job.columns * 2, 0.F);
        std::vector<bool> seen(job.columns, false);
        const auto& range = timeline->getTimeRange();
        const double origin = range.start_time().rescaled_to(1.0).value();
        const double start = origin + job.start;
        const double end = start + job.duration;
        std::shared_ptr<tl::AudioResample> resample;
        for (double second = std::floor(start); second < end && !job.cancelled; second += 1.0)
        {
            const auto frame = timeline->getAudio(second).future.get();
            for (const auto& layer : frame.layers)
            {
                if (!layer.audio)
                    continue;
                const auto& info = layer.audio->getInfo();
                const tl::AudioInfo f32(info.channelCount, tl::AudioType::F32, info.sampleRate);
                if (!resample || resample->getInputInfo() != info)
                {
                    resample = tl::AudioResample::create(info, f32);
                }
                const auto audio = info.type == tl::AudioType::F32 ?
                    layer.audio :
                    resample->process(layer.audio);
                if (!audio)
                    continue;
                const float* data = reinterpret_cast<const float*>(audio->getData());
                const size_t count = audio->getSampleCount();
                const int channels = audio->getChannelCount();
                const double rate = audio->getSampleRate();
                for (size_t i = 0; i < count; ++i)
                {
                    const double t = second + i / rate;
                    if (t < start || t >= end)
                        continue;
                    const int column = std::min(
                        static_cast<int>((t - start) / job.duration * job.columns),
                        job.columns - 1);
                    float& low = out[column * 2];
                    float& high = out[column * 2 + 1];
                    for (int c = 0; c < channels; ++c)
                    {
                        const float v = data[i * channels + c];
                        if (!seen[column])
                        {
                            low = high = v;
                            seen[column] = true;
                        }
                        else
                        {
                            low = std::min(low, v);
                            high = std::max(high, v);
                        }
                    }
                }
            }
        }
        return out;
    }

    void waveformRun(tl_player* player)
    {
        // Read through the player's own timeline. A timeline of its own
        // mapped the whole .otioz again, and on an iPad three mappings of
        // a 1.6 GB bundle (the player's, the thumbnails', this) ran the
        // process out of address space. The player cancels its requests
        // by id, so a seek leaves these alone.
        const std::shared_ptr<tl::Timeline> timeline = player->timeline;
        while (player->waveformRunning)
        {
            std::shared_ptr<WaveformJob> job;
            {
                std::unique_lock<std::mutex> lock(player->waveformMutex);
                player->waveformCV.wait(lock, [player]
                {
                    return !player->waveformRunning || !player->waveformQueue.empty();
                });
                if (!player->waveformRunning)
                    break;
                job = player->waveformQueue.front();
                player->waveformQueue.pop_front();
            }
            std::vector<float> columns;
            if (timeline && !job->cancelled)
            {
                try
                {
                    columns = readWaveform(timeline, *job);
                }
                catch (const std::exception&)
                {}
            }
            job->promise.set_value(job->cancelled ? std::vector<float>() : columns);
            std::unique_lock<std::mutex> lock(player->waveformMutex);
            player->waveformJobs.erase(job->id);
        }
        // What is left in the queue is not going to be read.
        std::unique_lock<std::mutex> lock(player->waveformMutex);
        for (const auto& job : player->waveformQueue)
        {
            job->promise.set_value(std::vector<float>());
        }
        player->waveformQueue.clear();
    }
}
}

unsigned long long tl_player_request_waveform(
    tl_player* player, double start, double duration, int columns)
{
    if (!player || duration <= 0.0 || columns <= 0)
        return 0;
    if (player->timeline->getIOInfo().audio.sampleRate <= 0)
        return 0;
    if (!player->waveformThread.joinable())
    {
        player->waveformRunning = true;
        player->waveformThread = std::thread(waveformRun, player);
    }
    auto job = std::make_shared<WaveformJob>();
    job->start = start;
    job->duration = duration;
    job->columns = columns;
    {
        std::unique_lock<std::mutex> lock(player->waveformMutex);
        job->id = ++player->waveformId;
        player->waveforms[job->id] = job->promise.get_future();
        player->waveformJobs[job->id] = job;
        player->waveformQueue.push_back(job);
    }
    player->waveformCV.notify_one();
    return job->id;
}

int tl_player_get_waveform(tl_player* player, unsigned long long id, float* columns, int count)
{
    if (!player || !columns)
        return -1;
    const auto i = player->waveforms.find(id);
    if (i == player->waveforms.end())
        return -1;
    if (i->second.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return 0;
    const auto values = i->second.get();
    player->waveforms.erase(i);
    if (values.empty())
        return -1;
    for (int c = 0; c < count * 2; ++c)
    {
        columns[c] = c < static_cast<int>(values.size()) ? values[c] : 0.F;
    }
    return 1;
}

void tl_player_cancel_waveform(tl_player* player, unsigned long long id)
{
    if (!player)
        return;
    player->waveforms.erase(id);
    std::unique_lock<std::mutex> lock(player->waveformMutex);
    const auto i = player->waveformJobs.find(id);
    if (i != player->waveformJobs.end())
    {
        i->second->cancelled = true;
    }
}

tl_timeline* tl_timeline_open(tl_engine* engine, const char* path)
{
    if (!engine || !engine->context || !path)
        return nullptr;
    auto out = new tl_timeline;
    out->engine = engine;
    try
    {
        out->timeline = tl::Timeline::create(engine->context, ftk::Path(openPath(path)));
        listClips(out->timeline, out->clips, out->videoTracks, nullptr);
    }
    catch (const std::exception&)
    {
        delete out;
        out = nullptr;
    }
    return out;
}

void tl_timeline_close(tl_timeline* timeline)
{
    delete timeline;
}

double tl_timeline_get_duration(tl_timeline* timeline)
{
    return timeline ? timeline->timeline->getTimeRange().duration().rescaled_to(1.0).value() : 0.0;
}

double tl_timeline_get_rate(tl_timeline* timeline)
{
    return timeline ? timeline->timeline->getTimeRange().duration().rate() : 0.0;
}

int tl_timeline_get_video_width(tl_timeline* timeline)
{
    if (!timeline)
        return 0;
    const auto& info = timeline->timeline->getIOInfo();
    return !info.video.empty() ? info.video.front().size.w : 0;
}

int tl_timeline_get_video_height(tl_timeline* timeline)
{
    if (!timeline)
        return 0;
    const auto& info = timeline->timeline->getIOInfo();
    return !info.video.empty() ? info.video.front().size.h : 0;
}

int tl_timeline_get_clip_count(tl_timeline* timeline)
{
    return timeline ? static_cast<int>(timeline->clips.size()) : 0;
}

int tl_timeline_get_clip(tl_timeline* timeline, int index, tl_clip* out)
{
    if (!timeline || !out || index < 0 || index >= static_cast<int>(timeline->clips.size()))
        return 0;
    *out = timeline->clips[index];
    return 1;
}


unsigned long long tl_timeline_request_poster(tl_timeline* timeline, double seconds, int height)
{
    if (!timeline || height <= 0)
        return 0;
    auto thumbnails = timeline->engine->context->getSystem<tl::ui::ThumbnailSystem>();
    if (!thumbnails)
        return 0;
    const auto& range = timeline->timeline->getTimeRange();
    const double rate = range.duration().rate();
    // The browser's kind: the file is not kept open for the next
    // request, as the timeline widget's are. A library of bundles kept
    // open would map every one of them.
    auto request = thumbnails->getThumbnail(
        timeline->timeline->getPath(),
        height,
        range.start_time() + OTIO_NS::RationalTime(seconds * rate, rate).round(),
        tl::IOOptions(),
        tl::ui::ThumbnailType::Browser);
    std::unique_lock<std::mutex> lock(timeline->engine->posterMutex);
    timeline->engine->posters[request.id] = std::move(request.future);
    return request.id;
}

int tl_engine_get_poster(tl_engine* engine, unsigned long long id, tl_image* out)
{
    if (!engine || !out)
        return -1;
    std::future<std::shared_ptr<ftk::Image> > future;
    {
        std::unique_lock<std::mutex> lock(engine->posterMutex);
        const auto i = engine->posters.find(id);
        if (i == engine->posters.end())
            return -1;
        if (i->second.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return 0;
        future = std::move(i->second);
        engine->posters.erase(i);
    }
    std::shared_ptr<ftk::Image> image;
    try
    {
        image = future.get();
    }
    catch (const std::exception&)
    {}
    return copyImage(image, out) ? 1 : -1;
}

int tl_player_get_video_width(tl_player* player)
{
    if (!player)
        return 0;
    const auto& info = player->player->getIOInfo();
    return !info.video.empty() ? info.video.front().size.w : 0;
}

int tl_player_get_video_height(tl_player* player)
{
    if (!player)
        return 0;
    const auto& info = player->player->getIOInfo();
    return !info.video.empty() ? info.video.front().size.h : 0;
}

namespace
{
    //! Finish a view once SDL has a window for it: claim the window for
    //! the device and make the renderer.
    tl_view* viewInit(tl_engine* engine, SDL_PropertiesID props)
    {
        auto out = new tl_view;
        out->engine = engine;
        try
        {
            SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_METAL_BOOLEAN, true);
            SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
            out->window = SDL_CreateWindowWithProperties(props);
            SDL_DestroyProperties(props);
            if (!out->window)
            {
                throw std::runtime_error(std::string("Cannot make the view: ") + SDL_GetError());
            }
            SDL_GPUDevice* device = engine->gpu->getDevice();
            out->claimed = SDL_ClaimWindowForGPUDevice(device, out->window);
            if (!out->claimed)
            {
                throw std::runtime_error(std::string("Cannot claim the view: ") + SDL_GetError());
            }
            out->present = ftk::gpu::Present::create(engine->gpu);
            out->composition = ftk::gpu::setComposition(
                engine->gpu,
                out->window,
                ftk::gpu::getComposition(engine->gpu, out->window));
            out->render = tl::gpu::Render::create(
                engine->gpu,
                engine->context->getLogSystem(),
                engine->context->getSystem<ftk::FontSystem>());
            out->widgets = WidgetHost::create(engine->context);
            engine->context->log(
                "tl_view",
                ftk::Format("Swapchain: {0}").arg(ftk::gpu::getLabel(out->composition)));
        }
        catch (const std::exception& e)
        {
            setError(engine, e.what());
            tl_view_destroy(out);
            out = nullptr;
        }
        return out;
    }
}

tl_view* tl_view_create(tl_engine* engine, void* nsWindow, void* nsView)
{
    if (!engine || !engine->gpu)
        return nullptr;
    // SDL wraps the native view rather than making a window of its own:
    // the swapchain is its, the layout and the events stay the
    // application's.
    const SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_COCOA_WINDOW_POINTER, nsWindow);
    SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_COCOA_VIEW_POINTER, nsView);
    return viewInit(engine, props);
}

tl_view* tl_view_create_ios(tl_engine* engine, void* uiWindowScene)
{
    if (!engine || !engine->gpu)
        return nullptr;
    // SDL makes a window of its own in the scene; the application
    // layers it under its own and tells the view where the picture
    // goes.
    const SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WINDOWSCENE_POINTER, uiWindowScene);
    return viewInit(engine, props);
}

void* tl_view_get_native_window(tl_view* view)
{
    if (!view || !view->window)
        return nullptr;
    const SDL_PropertiesID props = SDL_GetWindowProperties(view->window);
#if TARGET_OS_IOS
    return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr);
#else
    return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
#endif
}

void tl_view_set_frame(tl_view* view, int x, int y, int w, int h)
{
    if (view)
    {
        const ftk::Box2I frame(x, y, w, h);
        if (frame != view->frame)
        {
            view->frame = frame;
            view->dirty = true;
        }
    }
}

// C++ linkage: this part of the file is inside the C interface's
// extern "C".
extern "C++"
{
namespace
{
    std::vector<std::pair<double, const void*> > frameSignature(const std::vector<tl::VideoFrame>& frames)
    {
        std::vector<std::pair<double, const void*> > out;
        for (const auto& frame : frames)
        {
            const void* image = !frame.layers.empty() ? frame.layers[0].image.get() : nullptr;
            out.push_back(std::make_pair(frame.time.value(), image));
        }
        return out;
    }
}
}

size_t tl_view_get_draw_count(tl_view* view)
{
    return view ? view->drawCount : 0;
}

int tl_view_needs_draw(tl_view* view)
{
    if (!view || !view->render)
        return 0;
    // The thumbnails yield to playback; see ThumbnailSystem::setPlaybackHold().
    if (auto thumbnails = view->engine->context->getSystem<tl::ui::ThumbnailSystem>())
    {
        thumbnails->setPlaybackHold(
            view->player &&
            view->player->player->observePlayback()->get() != tl::Playback::Stop);
    }
    if (view->widgets)
    {
        view->widgets->setFrames(
            ftk::Size2I(view->drawnSize), view->timelineFrame, view->displayScale);
        view->widgets->tick();
        if (view->widgets->needsDraw())
        {
            view->dirty = true;
        }
    }
    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(view->window, &w, &h);
    if (ftk::Size2I(w, h) != view->drawnSize)
    {
        view->dirty = true;
    }
    if (view->player)
    {
        const auto frames = view->player->player->observeCurrentVideo()->get();
        if (frameSignature(frames) != view->drawnFrames)
        {
            view->dirty = true;
        }
    }
    return view->dirty ? 1 : 0;
}


void tl_view_set_timeline_frame(tl_view* view, int x, int y, int w, int h, float displayScale)
{
    if (view)
    {
        const ftk::Box2I frame(x, y, w, h);
        if (frame != view->timelineFrame || displayScale != view->displayScale)
        {
            view->timelineFrame = frame;
            view->displayScale = displayScale;
            view->dirty = true;
        }
    }
}

void tl_view_set_display_scale(tl_view* view, float value)
{
    if (view && value > 0.F && value != view->displayScale)
    {
        view->displayScale = value;
        view->dirty = true;
    }
}

void tl_view_set_timeline_minimized(tl_view* view, int value)
{
    if (view && view->widgets)
    {
        view->widgets->setMinimized(value != 0);
        view->dirty = true;
    }
}

void tl_view_get_timeline_size_hint(tl_view* view, int* w, int* h)
{
    const ftk::Size2I size = view && view->widgets ?
        view->widgets->getTimelineSizeHint() :
        ftk::Size2I();
    if (w)
        *w = size.w;
    if (h)
        *h = size.h;
}

void tl_view_get_view(tl_view* view, float* x, float* y, float* zoom)
{
    if (x)
        *x = view ? view->viewPosNow.x : 0.F;
    if (y)
        *y = view ? view->viewPosNow.y : 0.F;
    if (zoom)
        *zoom = view ? view->viewZoomNow : 1.F;
}

void tl_view_set_view(tl_view* view, float x, float y, float zoom)
{
    if (!view || zoom <= 0.F)
        return;
    view->viewFramed = false;
    view->viewPos = ftk::V2F(x, y);
    view->viewZoom = zoom;
    view->viewPosNow = view->viewPos;
    view->viewZoomNow = zoom;
    view->dirty = true;
}

void tl_view_frame_view(tl_view* view)
{
    if (!view)
        return;
    view->viewFramed = true;
    view->dirty = true;
}

void tl_view_zoom(tl_view* view, float factor, float x, float y)
{
    if (!view || factor <= 0.F)
        return;
    // The point under the fingers stays under them.
    const ftk::V2F pos = view->viewPosNow;
    const float zoom = view->viewZoomNow;
    tl_view_set_view(
        view,
        x - (x - pos.x) * factor,
        y - (y - pos.y) * factor,
        zoom * factor);
}

void tl_view_pan(tl_view* view, float dx, float dy)
{
    if (!view)
        return;
    tl_view_set_view(
        view,
        view->viewPosNow.x + dx,
        view->viewPosNow.y + dy,
        view->viewZoomNow);
}

void tl_view_gesture(
    tl_view* view, tl_touch_phase phase, int x, int y, float panX, float panY, float zoom)
{
    if (!view || !view->widgets)
        return;
    switch (phase)
    {
    case TL_TOUCH_BEGAN:
    case TL_TOUCH_MOVED:
        view->widgets->gesture(ftk::V2I(x, y), ftk::V2F(panX, panY), zoom);
        break;
    case TL_TOUCH_ENDED:
    case TL_TOUCH_CANCELLED:
        view->widgets->gestureEnd();
        break;
    }
}

void tl_view_touch(tl_view* view, tl_touch_phase phase, int x, int y)
{
    if (!view || !view->widgets)
        return;
    const ftk::V2I pos(x, y);
    switch (phase)
    {
    case TL_TOUCH_BEGAN: view->widgets->press(pos); break;
    case TL_TOUCH_MOVED: view->widgets->hover(pos); break;
    case TL_TOUCH_ENDED:
    case TL_TOUCH_CANCELLED: view->widgets->release(pos); break;
    }
}

void tl_view_destroy(tl_view* view)
{
    if (!view)
        return;
    view->widgets.reset();
    view->buffer.reset();
    view->render.reset();
    view->present.reset();
    if (view->window)
    {
        if (view->claimed)
        {
            SDL_ReleaseWindowFromGPUDevice(view->engine->gpu->getDevice(), view->window);
        }
        SDL_DestroyWindow(view->window);
    }
    delete view;
}

void tl_view_set_player(tl_view* view, tl_player* player)
{
    if (view)
    {
        view->player = player;
        view->dirty = true;
        if (view->widgets)
        {
            view->widgets->setPlayer(player ? player->player : nullptr);
        }
    }
}

void tl_view_draw(tl_view* view)
{
    if (!view || !view->render)
        return;
    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(view->window, &w, &h);
    if (w <= 0 || h <= 0)
        return;
    const ftk::Size2I size(w, h);
    try
    {
        auto& gpu = view->engine->gpu;
        if (!view->buffer || view->buffer->getSize() != size)
        {
            view->buffer = ftk::gpu::OffscreenBuffer::create(
                gpu, size, ftk::gpu::BufferType::RGBA_F16);
        }

        std::vector<tl::VideoFrame> frames;
        if (view->player)
        {
            frames = view->player->player->observeCurrentVideo()->get();
        }
        view->drawnFrames = frameSignature(frames);
        view->drawnSize = size;
        view->dirty = false;
        ++view->drawCount;

        // The picture fitted to the frame, or the whole view, as the
        // viewport frames it.
        const ftk::Box2I frame = view->frame.isValid() ?
            view->frame :
            ftk::Box2I(0, 0, w, h);
        const tl::CompareOptions& compare = view->compareOptions;
        const tl::AspectRatioOptions aspect;
        const auto boxes = tl::getBoxes(compare, aspect, frames);
        const ftk::Size2I renderSize = tl::getRenderSize(compare, aspect, frames);
        float zoom = 1.F;
        ftk::V2F pos(frame.x(), frame.y());
        if (!view->viewFramed)
        {
            pos = view->viewPos;
            zoom = view->viewZoom;
        }
        else if (renderSize.isValid())
        {
            zoom = std::min(
                static_cast<float>(frame.w()) / renderSize.w,
                static_cast<float>(frame.h()) / renderSize.h);
            pos.x += (frame.w() - renderSize.w * zoom) / 2.F;
            pos.y += (frame.h() - renderSize.h * zoom) / 2.F;
        }
        view->viewPosNow = pos;
        view->viewZoomNow = zoom;
        view->picture = tl::c::ViewPicture();
        view->picture.pos = pos;
        view->picture.zoom = zoom;
        view->picture.displayScale = view->displayScale;
        if (!boxes.empty())
        {
            view->picture.box = boxes.front();
        }
        if (!frames.empty() &&
            !frames.front().layers.empty() &&
            frames.front().layers.front().image)
        {
            view->picture.imageSize = frames.front().layers.front().image->getSize();
        }
        const ftk::M44F vm =
            ftk::translate(ftk::V3F(pos.x, pos.y, 0.F)) *
            ftk::scale(ftk::V3F(zoom, zoom, 1.F));
        const ftk::M44F pm = ftk::ortho(
            0.F, static_cast<float>(w), static_cast<float>(h), 0.F, -1.F, 1.F);

        auto base = view->render->getBaseRender();
        base->setTarget(view->buffer);
        view->render->begin(size);
        // The background behind the picture, the picture, and the
        // foreground over it, as the viewport draws them, into the one
        // buffer here. The color options are given every draw; the
        // renderer keeps its processors when they have not changed.
        view->render->setOCIOOptions(view->ocioOptions);
        view->render->setOCIOInputResolver(view->ocioInputResolver);
        view->render->setLUTOptions(view->lutOptions);
        view->render->setTransform(pm);
        view->render->drawBackground(boxes, vm, view->backgroundOptions, compare);
        view->render->setTransform(pm * vm);
        view->render->drawVideo(
            frames,
            boxes,
            { view->imageOptions },
            { view->displayOptions },
            compare,
            ftk::ImageType::RGBA_F16);
        view->render->setTransform(pm);
        view->render->drawForeground(boxes, vm, view->foregroundOptions, compare);
        if (view->overlay)
        {
            view->render->setTransform(pm);
            view->overlay(view->render);
        }
        // The widgets over the picture, in the buffer's own coordinates.
        if (view->widgets)
        {
            view->render->setTransform(pm);
            view->widgets->setFrames(size, view->timelineFrame, view->displayScale);
            view->widgets->draw(view->render);
        }
        view->render->end();

        // To the view's swapchain: the texture is the view's for this
        // frame only, and submitting the command buffer presents it.
        SDL_GPUDevice* device = gpu->getDevice();
        SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
        SDL_GPUTexture* swapchain = nullptr;
        Uint32 sw = 0;
        Uint32 sh = 0;
        if (cmd &&
            SDL_WaitAndAcquireGPUSwapchainTexture(cmd, view->window, &swapchain, &sw, &sh) &&
            swapchain)
        {
            view->present->draw(
                cmd,
                view->buffer->getTexture(),
                swapchain,
                static_cast<int>(SDL_GetGPUSwapchainTextureFormat(device, view->window)),
                view->composition,
                ftk::gpu::getSDRWhiteLevel(view->window, view->composition));
        }
        if (cmd)
        {
            SDL_SubmitGPUCommandBuffer(cmd);
        }
    }
    catch (const std::exception& e)
    {
        setError(view->engine, e.what());
    }
}

}

namespace tl
{
    namespace c
    {
        std::shared_ptr<ftk::Context> getContext(tl_engine* engine)
        {
            return engine ? engine->context : nullptr;
        }

        std::shared_ptr<Timeline> getTimeline(tl_player* player)
        {
            return player ? player->timeline : nullptr;
        }

        std::shared_ptr<Player> getPlayer(tl_player* player)
        {
            return player ? player->player : nullptr;
        }

        void setOCIOOptions(tl_view* view, const OCIOOptions& value)
        {
            if (view && view->ocioOptions != value)
            {
                view->ocioOptions = value;
                view->dirty = true;
            }
        }

        void setOCIOInputResolver(
            tl_view* view,
            const std::function<std::string(const std::string&, const ftk::ImageTags&)>& value)
        {
            if (view)
            {
                view->ocioInputResolver = value;
                view->dirty = true;
            }
        }

        void setLUTOptions(tl_view* view, const LUTOptions& value)
        {
            if (view && view->lutOptions != value)
            {
                view->lutOptions = value;
                view->dirty = true;
            }
        }

        void setImageOptions(tl_view* view, const ftk::ImageOptions& value)
        {
            if (view && view->imageOptions != value)
            {
                view->imageOptions = value;
                view->dirty = true;
            }
        }

        void setDisplayOptions(tl_view* view, const DisplayOptions& value)
        {
            if (view && view->displayOptions != value)
            {
                view->displayOptions = value;
                view->dirty = true;
            }
        }

        void setBackgroundOptions(tl_view* view, const BackgroundOptions& value)
        {
            if (view && view->backgroundOptions != value)
            {
                view->backgroundOptions = value;
                view->dirty = true;
            }
        }

        void setForegroundOptions(tl_view* view, const ForegroundOptions& value)
        {
            if (view && view->foregroundOptions != value)
            {
                view->foregroundOptions = value;
                view->dirty = true;
            }
        }

        void setCompareOptions(tl_view* view, const CompareOptions& value)
        {
            if (view && view->compareOptions != value)
            {
                view->compareOptions = value;
                view->dirty = true;
            }
        }

        ViewPicture getViewPicture(tl_view* view)
        {
            return view ? view->picture : ViewPicture();
        }

        void setOverlay(
            tl_view* view,
            const std::function<void(const std::shared_ptr<ftk::IRender>&)>& value)
        {
            if (view)
            {
                view->overlay = value;
                view->dirty = true;
            }
        }

        void setNeedsDraw(tl_view* view)
        {
            if (view)
            {
                view->dirty = true;
            }
        }
    }
}

// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

// A minimal player: a viewport, a timeline, and transport controls.
// On the desktop it plays whatever the build's plugins read; on the
// web it plays movies through the WebCodecs plugin, and the shell
// forwards the page's query string as the command line, so
// "?url=clip.mp4" chooses the file.

#include <tlRender/UI/FrameToolBar.h>
#include <tlRender/UI/Init.h>
#include <tlRender/UI/PlaybackToolBar.h>
#include <tlRender/UI/TimeEdit.h>
#include <tlRender/UI/TimeLabel.h>
#include <tlRender/UI/TimeUnitsWidget.h>
#include <tlRender/UI/TimelineWidget.h>
#include <tlRender/UI/Viewport.h>

#include <tlRender/Timeline/Player.h>
#include <tlRender/Timeline/TimeUnits.h>

#include <ftk/UI/App.h>
#include <ftk/UI/RowLayout.h>
#include <ftk/UI/Window.h>

#include <ftk/Core/CmdLine.h>
#include <ftk/Core/Timer.h>

#include <atomic>
#include <iostream>
#include <thread>

using namespace ftk;

namespace
{
    //! Opening blocks on the readers' information, and on the web the
    //! WebCodecs reader cannot answer until the main thread's event
    //! loop runs -- so the open happens on a thread and the main
    //! thread polls for the result.
    struct Open
    {
        std::atomic<bool> done{ false };
        std::shared_ptr<tl::Timeline> timeline;
        std::shared_ptr<tl::Player> player;
        std::shared_ptr<ftk::Observer<OTIO_NS::RationalTime> > currentTimeObserver;
        std::thread thread;
    };
}

int main(int argc, char** argv)
{
    try
    {
        auto context = Context::create();
        tl::ui::init(context);
        auto urlOption = CmdLineOption<std::string>::create(
            { "-url" },
            "The file to play.",
            "Player");
        auto app = App::create(
            context,
            argc,
            argv,
            "player",
            "Example player.",
            {},
            { urlOption });
        if (app->hasCmdLineHelp())
            return 0;

        auto window = Window::create(context, app, "player");
        auto layout = VerticalLayout::create(context, window);
        layout->setSpacingRole(SizeRole::None);
        auto viewport = tl::ui::Viewport::create(context, layout);
        viewport->setVStretch(Stretch::Expanding);
        auto timeUnitsModel = tl::TimeUnitsModel::create(context);
        auto timelineWidget = tl::ui::TimelineWidget::create(
            context, timeUnitsModel, layout);
        auto bottomLayout = HorizontalLayout::create(context, layout);
        bottomLayout->setSpacingRole(SizeRole::SpacingSmall);
        auto playbackToolBar = tl::ui::PlaybackToolBar::create(
            context, bottomLayout);
        auto frameToolBar = tl::ui::FrameToolBar::create(
            context, bottomLayout);
        auto timeEdit = tl::ui::TimeEdit::create(
            context, timeUnitsModel, bottomLayout);
        auto durationLabel = tl::ui::TimeLabel::create(
            context, timeUnitsModel, bottomLayout);
        auto timeUnitsWidget = tl::ui::TimeUnitsWidget::create(
            context, timeUnitsModel, bottomLayout);

        const std::string url =
            urlOption->found() ? urlOption->getValue() : "test.mp4";
        auto open = std::make_shared<Open>();
        open->thread = std::thread(
            [open, context, url]
            {
                try
                {
                    Path path(url);
                    const auto seq = findSeq(path);
                    if (!seq.empty())
                    {
                        path.setSeq(seq);
                    }
                    open->timeline = tl::Timeline::create(context, path);
                }
                catch (const std::exception& e)
                {
                    std::cout << "ERROR: " << e.what() << std::endl;
                }
                open->done = true;
            });

        auto timer = Timer::create(context);
        timer->setRepeating(true);
        timer->start(
            std::chrono::milliseconds(100),
            [open, context, viewport, timelineWidget, playbackToolBar,
                frameToolBar, timeEdit, durationLabel, timer]
            {
                if (open->done && !open->player)
                {
                    open->thread.join();
                    timer->stop();
                    if (open->timeline)
                    {
                        tl::PlayerOptions playerOptions;
#if defined(__EMSCRIPTEN__)
                        // The wasm heap tops out at 2GB; the desktop
                        // cache default would spend it all and the
                        // browser kills the page.
                        playerOptions.cache.videoGB = .5F;
                        playerOptions.cache.audioGB = .1F;
#endif
                        open->player = tl::Player::create(
                            context, open->timeline, playerOptions);
                        viewport->setPlayer(open->player);
                        timelineWidget->setPlayer(open->player);
                        playbackToolBar->setPlayer(open->player);
                        frameToolBar->setPlayer(open->player);
                        timeEdit->setCallback(
                            [open](const OTIO_NS::RationalTime& value)
                            {
                                if (open->player)
                                {
                                    open->player->seek(value);
                                }
                            });
                        open->currentTimeObserver =
                            ftk::Observer<OTIO_NS::RationalTime>::create(
                                open->player->observeCurrentTime(),
                                [timeEdit](const OTIO_NS::RationalTime& value)
                                {
                                    timeEdit->setValue(value);
                                });
                        durationLabel->setValue(
                            open->player->getTimeRange().duration());
                    }
                }

            });

        app->run();
    }
    catch (const std::exception& e)
    {
        std::cout << "ERROR: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

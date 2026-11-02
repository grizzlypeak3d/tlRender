// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UITest/GestureTest.h>

#include <tlRender/UI/TimelineItem.h>
#include <tlRender/UI/TimelineWidget.h>
#include <tlRender/UI/Viewport.h>

#include <tlRender/Timeline/Player.h>
#include <tlRender/Timeline/Timeline.h>

#include <tlRender/IO/System.h>

#include <ftk/UI/App.h>
#include <ftk/UI/Window.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/Time.h>

#include <chrono>
#include <cmath>
#include <thread>

namespace tl
{
    namespace ui_tests
    {
        GestureTest::GestureTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "ui_tests::GestureTest")
        {}

        std::shared_ptr<GestureTest> GestureTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<GestureTest>(new GestureTest(context));
        }

        void GestureTest::run()
        {
            // One app for both: destroying it shuts the context down.
            std::vector<std::string> argv;
            argv.push_back("GestureTest");
            auto app = ftk::App::create(_context, argv, "GestureTest", "Gesture test.");
            _viewport(app);
            _timeline(app);
            _scrubPlayback(app);
            _scrubOutrun(app);
            _viewCommands(app);
        }

        void GestureTest::_viewport(const std::shared_ptr<ftk::App>& app)
        {
            auto window = ftk::Window::create(_context, app, "GestureTest");
            auto viewport = ui::Viewport::create(_context, window);
            window->show();
            window->layout(ftk::Size2I(1280, 960));
            app->tick();
            const ftk::Box2I& g = viewport->getGeometry();
            _print(ftk::Format("Viewport: {0}").arg(g));

            // A pinch zooms around the point between the fingers: that
            // point stays where it is.
            viewport->setViewPosAndZoom(ftk::V2I(0, 0), 1.0);
            const ftk::V2I focus(g.min.x + 100, g.min.y + 100);
            window->gesture(focus, ftk::V2F(), 2.F);
            auto viewPosZoom = viewport->getViewPosAndZoom();
            _print(ftk::Format("Pinch: {0} {1}").arg(viewPosZoom.first).arg(viewPosZoom.second));
            FTK_CHECK(2.0 == viewPosZoom.second);
            FTK_CHECK(ftk::V2I(-100, -100) == viewPosZoom.first);

            // A drag moves the view with the fingers.
            window->gesture(focus, ftk::V2F(10.F, 20.F));
            viewPosZoom = viewport->getViewPosAndZoom();
            _print(ftk::Format("Drag: {0} {1}").arg(viewPosZoom.first).arg(viewPosZoom.second));
            FTK_CHECK(2.0 == viewPosZoom.second);
            FTK_CHECK(ftk::V2I(-90, -80) == viewPosZoom.first);

            // Fingers moving less than a pixel at a time still move it.
            for (int i = 0; i < 10; ++i)
            {
                window->gesture(focus, ftk::V2F(.25F, 0.F));
            }
            viewPosZoom = viewport->getViewPosAndZoom();
            _print(ftk::Format("Slow drag: {0}").arg(viewPosZoom.first));
            FTK_CHECK(std::abs(viewPosZoom.first.x - -87) <= 1);

            // Not with the input disabled.
            viewport->setInputEnabled(false);
            window->gesture(focus, ftk::V2F(100.F, 100.F), 2.F);
            FTK_CHECK(viewPosZoom == viewport->getViewPosAndZoom());
            viewport->setInputEnabled(true);

            // The first finger's press is cancelled when the second comes
            // down: the pan it started is undone, and does not go on with
            // the gesture.
            viewport->setViewPosAndZoom(ftk::V2I(0, 0), 1.0);
            viewport->setPanBinding(ftk::MouseButton::Left, ftk::KeyModifier::None);
            window->drag({ focus, ftk::V2I(focus.x + 10, focus.y) }, 0, false);
            FTK_CHECK(ftk::V2I(10, 0) == viewport->getViewPosAndZoom().first);
            window->gesture(focus, ftk::V2F(), 1.F);
            FTK_CHECK(ftk::V2I(0, 0) == viewport->getViewPosAndZoom().first);
            window->hover(ftk::V2I(focus.x + 50, focus.y));
            FTK_CHECK(ftk::V2I(0, 0) == viewport->getViewPosAndZoom().first);
        }

        void GestureTest::_timeline(const std::shared_ptr<ftk::App>& app)
        {
            auto window = ftk::Window::create(_context, app, "GestureTest");
            auto timelineWidget = ui::TimelineWidget::create(_context, window);
            auto player = Player::create(
                _context,
                Timeline::create(
                    _context,
                    ftk::Path(TLRENDER_SAMPLE_DATA, "Gap.otio")));
            timelineWidget->setPlayer(player);
            window->show();
            window->layout(ftk::Size2I(1280, 960));
            app->tick();
            const ftk::Box2I& g = timelineWidget->getGeometry();
            const ftk::V2I center = ftk::center(g);
            FTK_CHECK(timelineWidget->hasFrameView());
            const double zoom = timelineWidget->getViewZoom();
            _print(ftk::Format("Timeline zoom: {0}").arg(zoom));

            // A pinch zooms the time.
            window->gesture(center, ftk::V2F(), 2.F);
            app->tick();
            _print(ftk::Format("Pinch: {0}").arg(timelineWidget->getViewZoom()));
            FTK_CHECK(std::abs(timelineWidget->getViewZoom() - zoom * 2.0) < 1e-6);
            FTK_CHECK(!timelineWidget->hasFrameView());

            // Pinching out past the whole of it frames it again.
            window->gesture(center, ftk::V2F(), .25F);
            app->tick();
            _print(ftk::Format("Pinch out: {0}").arg(timelineWidget->getViewZoom()));
            FTK_CHECK(std::abs(timelineWidget->getViewZoom() - zoom) < 1e-6);
            FTK_CHECK(timelineWidget->hasFrameView());

            // The first finger's press sets the current time; cancelled by
            // the second, the time goes back.
            const OTIO_NS::RationalTime timePrev = player->getCurrentTime();
            const ftk::V2I ruler(center.x, g.min.y + 4);
            window->drag({ ruler, ruler }, 0, false);
            app->tick();
            _print(ftk::Format("Pressed time: {0}").arg(player->getCurrentTime().value()));
            FTK_CHECK(player->getCurrentTime() != timePrev);
            window->gesture(ruler, ftk::V2F(), 1.F);
            app->tick();
            FTK_CHECK(player->getCurrentTime() == timePrev);

            // Zoomed well in, with the press on the tracks and a move or
            // two before the second finger comes down (the first finger
            // outran the hold-back): the time goes back, and the pan
            // carries on from where the view was, with no jump.
            for (int i = 0; i < 4; ++i)
            {
                window->gesture(center, ftk::V2F(), 2.F);
                app->tick();
            }
            _print(ftk::Format("Zoomed: {0}, scroll {1}").
                arg(timelineWidget->getViewZoom()).
                arg(timelineWidget->getScrollPos().x));
            const OTIO_NS::RationalTime timePrev2 = player->getCurrentTime();
            const ftk::V2I scrollPrev = timelineWidget->getScrollPos();
            const ftk::V2I tracks(center.x, g.min.y + 60);
            window->press(tracks);
            window->hover(ftk::V2I(tracks.x + 20, tracks.y));
            window->hover(ftk::V2I(tracks.x + 40, tracks.y));
            app->tick();
            _print(ftk::Format("Pressed on the tracks: time {0}, scroll {1}").
                arg(player->getCurrentTime().value()).
                arg(timelineWidget->getScrollPos().x));
            FTK_CHECK(player->getCurrentTime() != timePrev2);
            int panned = 0;
            for (int i = 0; i < 5; ++i)
            {
                window->gesture(tracks, ftk::V2F(-30.F, 0.F), 1.F);
                panned += 30;
                app->tick();
                _print(ftk::Format("Pan {0}: time {1}, scroll {2}").
                    arg(i).
                    arg(player->getCurrentTime().value()).
                    arg(timelineWidget->getScrollPos().x));
            }
            for (int i = 0; i < 10; ++i)
            {
                app->tick();
            }
            _print(ftk::Format("After the pan: time {0}, scroll {1} (was {2}, panned {3})").
                arg(player->getCurrentTime().value()).
                arg(timelineWidget->getScrollPos().x).
                arg(scrollPrev.x).
                arg(panned));
            FTK_CHECK(player->getCurrentTime() == timePrev2);
            FTK_CHECK(timelineWidget->getScrollPos().x == scrollPrev.x + panned);
        }

        void GestureTest::_scrubPlayback(const std::shared_ptr<ftk::App>& app)
        {
            // Scrubbing while playing: the player is stopped while the
            // button is held, so the picture follows the pointer rather
            // than running ahead of it between moves, and plays on from
            // where the drag ends. With stop on scrub it stays stopped.
            auto window = ftk::Window::create(_context, app, "GestureTest");
            auto timelineWidget = ui::TimelineWidget::create(_context, window);
            auto player = Player::create(
                _context,
                Timeline::create(
                    _context,
                    ftk::Path(TLRENDER_SAMPLE_DATA, "Gap.otio")));
            timelineWidget->setPlayer(player);
            window->show();
            window->layout(ftk::Size2I(1280, 960));
            app->tick();
            const ftk::Box2I& g = timelineWidget->getGeometry();
            const ftk::V2I ruler(g.min.x + g.w() / 4, g.min.y + 4);
            const ftk::V2I ruler2(g.min.x + g.w() / 2, g.min.y + 4);
            // The track item, wherever the layout put it.
            std::function<std::shared_ptr<ui::TimelineItem>(const std::shared_ptr<ftk::IWidget>&)> findItem;
            findItem = [&findItem](const std::shared_ptr<ftk::IWidget>& widget)
                -> std::shared_ptr<ui::TimelineItem>
            {
                if (auto item = std::dynamic_pointer_cast<ui::TimelineItem>(widget))
                {
                    return item;
                }
                for (const auto& child : widget->getChildren())
                {
                    if (auto item = findItem(child))
                    {
                        return item;
                    }
                }
                return nullptr;
            };
            auto item = findItem(timelineWidget);
            FTK_CHECK(item);
            if (!item)
            {
                return;
            }
            const ftk::Box2I& ig = item->getGeometry();
            const ftk::V2I track(ig.min.x + ig.w() / 4, ig.min.y + 4);
            const ftk::V2I track2(ig.min.x + ig.w() / 2, ig.min.y + 4);

            for (const auto& points : { std::pair(ruler, ruler2), std::pair(track, track2) })
            {
                timelineWidget->setStopOnScrub(false);
                player->setPlayback(Playback::Forward);
                app->tick();
                window->press(points.first);
                app->tick();
                FTK_CHECK(Playback::Stop == player->getPlayback());
                window->hover(points.second);
                app->tick();
                FTK_CHECK(Playback::Stop == player->getPlayback());
                window->release(points.second);
                app->tick();
                FTK_CHECK(Playback::Forward == player->getPlayback());
                player->stop();

                timelineWidget->setStopOnScrub(true);
                player->setPlayback(Playback::Forward);
                app->tick();
                window->drag({ points.first, points.second });
                app->tick();
                FTK_CHECK(Playback::Stop == player->getPlayback());
            }
            player->stop();
        }

        void GestureTest::_scrubOutrun(const std::shared_ptr<ftk::App>& app)
        {
            // A drag that outruns the reader waits for the frame it is on
            // and then asks for where the pointer has got to, and the
            // picture never lands past the pointer: landing on the keyframe
            // before the time instead, as it did for a while, put it a
            // group of pictures beyond the finger when dragging backward,
            // and the picture flashed back at every group crossed.
            auto window = ftk::Window::create(_context, app, "GestureTest");
            auto timelineWidget = ui::TimelineWidget::create(_context, window);
            const ftk::Path path(TLRENDER_SAMPLE_DATA, "BART_2021-02-07.m4v");
            if (!_context->getSystem<ReadSystem>()->getPlugin(path))
            {
                _print("Skipped: no plugin reads the fixture");
                return;
            }
            auto player = Player::create(
                _context,
                Timeline::create(_context, path));
            timelineWidget->setPlayer(player);
            window->show();
            window->layout(ftk::Size2I(1280, 960));
            app->tick();
            const ftk::Box2I& g = timelineWidget->getGeometry();
            const auto wait = [this, app](int ms)
            {
                const auto t = std::chrono::steady_clock::now();
                while (std::chrono::steady_clock::now() - t < std::chrono::milliseconds(ms))
                {
                    app->tick();
                    ftk::sleep(std::chrono::milliseconds(2));
                }
            };
            // Backward and then forward, moving several times between ticks
            // so that the reader is outrun and the keyframes are used. The
            // time at the pointer is what the widget says it is.
            for (const int dir : { -1, 1 })
            {
                const int x0 = dir < 0 ? g.min.x + g.w() * 9 / 10 : g.min.x + g.w() / 10;
                const int y = g.min.y + 40;
                // Until the press scrubs: the items are made once the
                // timeline has been read, and on a slow machine (a debug
                // build on a CI runner) the first press came before them
                // and landed on nothing.
                for (int attempt = 0; attempt < 100; ++attempt)
                {
                    window->press(ftk::V2I(x0, y));
                    wait(50);
                    if (timelineWidget->observeTimeScrub()->get().has_value())
                    {
                        break;
                    }
                    window->release(ftk::V2I(x0, y));
                    wait(50);
                }
                int x = x0;
                for (int i = 0; i < 40; ++i)
                {
                    for (int j = 0; j < 3; ++j)
                    {
                        x += dir * 6;
                        window->hover(ftk::V2I(x, y));
                    }
                    wait(20);
                    const auto pointer = timelineWidget->observeTimeScrub()->get();
                    FTK_CHECK(pointer.has_value());
                    if (pointer.has_value())
                    {
                        const OTIO_NS::RationalTime current = player->getCurrentTime();
                        const bool past = dir < 0 ? current < *pointer : current > *pointer;
                        if (past)
                        {
                            _print(ftk::Format("Direction {0}: shown {1}, pointer {2}").
                                arg(dir).arg(current.value()).arg(pointer->value()));
                        }
                        FTK_CHECK(!past);
                    }
                }
                window->release(ftk::V2I(x, y));
                wait(50);
            }
        }

        void GestureTest::_viewCommands(const std::shared_ptr<ftk::App>& app)
        {
            // Here because the suite can make only one app. Offscreen, so
            // that nothing is shown.
            auto window = ftk::Window::create(_context, app, "GestureTest");
            window->setOffscreen(true);
            auto viewport = ui::Viewport::create(_context, window);
            auto player = Player::create(
                _context,
                Timeline::create(
                    _context,
                    ftk::Path(TLRENDER_SAMPLE_DATA, "AudioToneNone.png")));
            viewport->setPlayer(player);
            window->show();
            window->layout(ftk::Size2I(1920, 1080));
            for (int i = 0; i < 200 && player->getCurrentVideo().empty(); ++i)
            {
                app->tick();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            const ftk::Box2I& g = viewport->getGeometry();
            _print(ftk::Format("View commands viewport: {0}").arg(g));
            FTK_CHECK(!player->getCurrentVideo().empty());
            FTK_CHECK(ftk::Size2I(1920, 1080) == g.size());
            FTK_CHECK(viewport->hasFrameView());

            // A framed view is already centered.
            viewport->center();
            FTK_CHECK(viewport->hasFrameView());

            // The 1920x1080 image framed in a viewport its size is already
            // 1:1. Resetting the zoom still takes frame view off; it did
            // not when the zoom it set was the one there (DJV #944).
            viewport->resetZoom();
            _print(ftk::Format("Reset zoom: {0} {1}").
                arg(viewport->getViewPos()).
                arg(viewport->getZoom()));
            FTK_CHECK(!viewport->hasFrameView());
            FTK_CHECK(1.0 == viewport->getZoom());
            FTK_CHECK(ftk::V2I(0, 0) == viewport->getViewPos());

            // Zooming in from a framed view starts from the framed zoom,
            // here 1:2, before a draw has worked it out.
            viewport->setFrameView(false);
            viewport->setViewPosAndZoom(ftk::V2I(0, 0), 1.0);
            window->layout(ftk::Size2I(960, 540));
            app->tick();
            viewport->setFrameView(true);
            viewport->zoomIn();
            _print(ftk::Format("Zoom in: {0}").arg(viewport->getZoom()));
            FTK_CHECK(!viewport->hasFrameView());
            FTK_CHECK(1.0 == viewport->getZoom());

            // A file of another size, not framed, is centered where the last
            // one was (DJV #931): the 1920x1080 image is centered at 1:1, so
            // the 400x300 one is too.
            FTK_CHECK(ftk::V2I(-480, -270) == viewport->getViewPos());
            auto player2 = Player::create(
                _context,
                Timeline::create(
                    _context,
                    ftk::Path(TLRENDER_SAMPLE_DATA, "ColorPattern.png")));
            viewport->setPlayer(player2);
            for (int i = 0; i < 200 && player2->getCurrentVideo().empty(); ++i)
            {
                app->tick();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            _print(ftk::Format("Other size: {0} {1}").
                arg(viewport->getViewPos()).
                arg(viewport->getZoom()));
            FTK_CHECK(!viewport->hasFrameView());
            FTK_CHECK(1.0 == viewport->getZoom());
            FTK_CHECK(ftk::V2I(480 - 200, 270 - 150) == viewport->getViewPos());
        }
    }
}

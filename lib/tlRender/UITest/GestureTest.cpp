// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UITest/GestureTest.h>

#include <tlRender/UI/TimelineWidget.h>
#include <tlRender/UI/Viewport.h>

#include <tlRender/Timeline/Player.h>
#include <tlRender/Timeline/Timeline.h>

#include <ftk/UI/App.h>
#include <ftk/UI/Window.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Format.h>

#include <cmath>

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
        }
    }
}

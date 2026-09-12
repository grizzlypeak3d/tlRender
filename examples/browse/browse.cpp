// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

// An experiment in browsing as previewing: a file browser beside a
// viewport, with the browser's selection driving what the viewport
// shows. Selecting a file tears down the player and builds a new one,
// and the time that takes is printed for each open -- the point of the
// experiment is to feel that cost, so the open is deliberately on the
// main thread with no debounce, none of the softening a real feature
// would have.

#include <tlRender/UI/Init.h>
#include <tlRender/UI/Viewport.h>

#include <tlRender/Timeline/Player.h>

#include <ftk/UI/Action.h>
#include <ftk/UI/App.h>
#include <ftk/UI/Divider.h>
#include <ftk/UI/FileBrowser.h>
#include <ftk/UI/FileBrowserWidgets.h>
#include <ftk/UI/Label.h>
#include <ftk/UI/MainWindow.h>
#include <ftk/UI/Menu.h>
#include <ftk/UI/MenuBar.h>
#include <ftk/UI/RowLayout.h>
#include <ftk/UI/ScrollWidget.h>
#include <ftk/UI/Splitter.h>

#include <ftk/Core/CmdLine.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/Path.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

using namespace ftk;

int main(int argc, char* argv[])
{
    int out = 1;
    try
    {
        auto context = Context::create();
        tl::ui::init(context);
        auto pathArg = CmdLineArg<std::string>::create(
            "Path",
            "Directory to browse.",
            true);
        auto fillOption = CmdLineOption<float>::create(
            { "-fill" },
            "Seconds to let each player fill its cache before the next "
            "open. The wait is what makes the teardown cost visible: a "
            "player torn down at once has queued nothing yet.",
            "Benchmark",
            0.F);
        auto app = App::create(
            context,
            argc,
            argv,
            "browse",
            "Example that previews the file browser selection.",
            { pathArg },
            { fillOption });
        if (app->hasCmdLineHelp())
            return 0;

        // MainWindow rather than Window for the standard menu bar --
        // File/Exit and the Window menu -- which an experiment gets for
        // free.
        auto window = MainWindow::create(context, app);
        window->setTitle("browse");

        auto layout = VerticalLayout::create(context);
        layout->setSpacingRole(SizeRole::None);
        window->setWidget(layout);

        std::filesystem::path startPath = std::filesystem::current_path();
        if (pathArg->hasValue())
        {
            startPath = ftk::toFileSystem(pathArg->getValue());
        }

        // Just the pieces browsing needs -- the path bar and the view --
        // rather than the whole dialog widget with its Ok and Cancel.
        auto model = FileBrowserModel::create(context);
        model->setPath(startPath);

        auto splitter = Splitter::create(context, Orientation::Horizontal, layout);
        splitter->setSplit(.3F);
        splitter->setVStretch(Stretch::Expanding);

        auto browserLayout = VerticalLayout::create(context, splitter);
        browserLayout->setSpacingRole(SizeRole::None);
        auto pathWidget = FileBrowserPath::create(context, browserLayout);
        auto view = FileBrowserView::create(
            context, FileBrowserMode::Open, model);
        auto scrollWidget = ScrollWidget::create(context);
        scrollWidget->setWidget(view);
        scrollWidget->setVStretch(Stretch::Expanding);
        scrollWidget->setParent(browserLayout);

        auto viewport = tl::ui::Viewport::create(context, splitter);

        // The viewport has no keys of its own; the application gives it
        // them, here as the shortcuts of a menu.
        std::weak_ptr<tl::ui::Viewport> viewportWeak(viewport);
        auto viewMenu = window->getMenuBar()->addMenu("View");
        viewMenu->addAction(Action::create(
            "Frame",
            KeyShortcut(Key::Backspace),
            [viewportWeak]
            {
                if (auto viewport = viewportWeak.lock())
                    viewport->setFrameView(true);
            }));
        viewMenu->addAction(Action::create(
            "Zoom 1:1",
            KeyShortcut(Key::_0),
            [viewportWeak]
            {
                if (auto viewport = viewportWeak.lock())
                    viewport->resetZoom();
            }));
        viewMenu->addAction(Action::create(
            "Zoom In",
            KeyShortcut(Key::Equals),
            [viewportWeak]
            {
                if (auto viewport = viewportWeak.lock())
                    viewport->zoomIn();
            }));
        viewMenu->addAction(Action::create(
            "Zoom Out",
            KeyShortcut(Key::Minus),
            [viewportWeak]
            {
                if (auto viewport = viewportWeak.lock())
                    viewport->zoomOut();
            }));

        Divider::create(context, Orientation::Vertical, layout);
        // The label says what is showing and what it cost, so the feel
        // and the number stay attached to each other.
        auto statusLabel = Label::create(
            context, "Select a file to preview it.", layout);
        statusLabel->setMarginRole(SizeRole::MarginSmall);

        pathWidget->setCallback(
            [model](const std::filesystem::path& value)
            {
                model->setPath(value);
            });
        auto pathObserver = Observer<std::filesystem::path>::create(
            model->observePath(),
            [pathWidget](const std::filesystem::path& value)
            {
                pathWidget->setPath(value);
            });

        std::shared_ptr<tl::Player> player;
        auto open = [context, viewport, statusLabel, &player](
            const std::vector<Path>& value)
        {
            if (1 != value.size())
                return;
            const Path& path = value.front();
            if (std::filesystem::is_directory(
                ftk::toFileSystem(path.get())))
                return;
            const auto t0 = std::chrono::steady_clock::now();
            try
            {
                // Timed apart from the create: this is where a switch
                // waits on the file being left, and it only shows once
                // the old player has had time to queue cache reads. The
                // viewport's reference goes first or the destructor runs
                // inside setPlayer and the number lands in the wrong
                // phase.
                viewport->setPlayer(nullptr);
                player.reset();
                const auto t0b = std::chrono::steady_clock::now();
                auto timeline = tl::Timeline::create(context, path);
                const auto t1 = std::chrono::steady_clock::now();
                player = tl::Player::create(context, timeline);
                const auto t1b = std::chrono::steady_clock::now();
                viewport->setPlayer(player);
                const auto t2 = std::chrono::steady_clock::now();
                const auto ms = [](const auto& a, const auto& b)
                {
                    return std::chrono::duration_cast<
                        std::chrono::milliseconds>(b - a).count();
                };
                const std::string text = Format(
                    "{0}: {1}ms (teardown {2}ms, timeline {3}ms, player {4}ms, setPlayer {5}ms)").
                    arg(path.getFileName()).
                    arg(ms(t0, t2)).
                    arg(ms(t0, t0b)).
                    arg(ms(t0b, t1)).
                    arg(ms(t1, t1b)).
                    arg(ms(t1b, t2));
                statusLabel->setText(text);
                std::cout << text << std::endl;
            }
            catch (const std::exception& e)
            {
                player.reset();
                viewport->setPlayer(nullptr);
                statusLabel->setText(e.what());
                std::cout << "ERROR: " << e.what() << std::endl;
            }
        };
        view->setSelectCallback(open);
        // Choosing (Return, double-click) previews too: in this
        // experiment there is nothing else for it to mean.
        view->setCallback(open);

        // A file argument instead of a directory runs the open several
        // times and exits: the same measurement, headless and
        // repeatable. The repeats show what stays warm between opens.
        if (pathArg->hasValue() &&
            !std::filesystem::is_directory(startPath))
        {
            for (int i = 0; i < 5; ++i)
            {
                open({ Path(pathArg->getValue()) });
                if (fillOption->getValue() > 0.F)
                {
                    // The player's own threads fill the cache; no event
                    // loop is needed for the reads to queue up.
                    std::this_thread::sleep_for(std::chrono::milliseconds(
                        static_cast<int>(fillOption->getValue() * 1000)));
                }
            }
            return 0;
        }

        window->show();
        app->run();
        out = 0;
    }
    catch (const std::exception& e)
    {
        std::cout << "ERROR: " << e.what() << std::endl;
    }
    return out;
}

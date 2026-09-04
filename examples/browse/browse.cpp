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

#include <ftk/UI/App.h>
#include <ftk/UI/Divider.h>
#include <ftk/UI/FileBrowser.h>
#include <ftk/UI/FileBrowserWidgets.h>
#include <ftk/UI/Label.h>
#include <ftk/UI/RowLayout.h>
#include <ftk/UI/ScrollWidget.h>
#include <ftk/UI/Splitter.h>
#include <ftk/UI/Window.h>

#include <ftk/Core/CmdLine.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/Path.h>

#include <chrono>
#include <filesystem>
#include <iostream>

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
        auto app = App::create(
            context,
            argc,
            argv,
            "browse",
            "Example that previews the file browser selection.",
            { pathArg });
        if (app->hasCmdLineHelp())
            return 0;

        auto window = Window::create(context, app, "browse");

        auto layout = VerticalLayout::create(context, window);
        layout->setSpacingRole(SizeRole::None);

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
                player.reset();
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
                    "{0}: {1}ms (timeline {2}ms, player {3}ms, setPlayer {4}ms)").
                    arg(path.getFileName()).
                    arg(ms(t0, t2)).
                    arg(ms(t0, t1)).
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

// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include "MainWindow.h"

#include "App.h"
#include "CompareActions.h"
#include "FileActions.h"
#include "FilesModel.h"
#include "MenuBar.h"
#include "PlaybackActions.h"
#include "PlaybackBar.h"
#include "SettingsModel.h"
#include "SettingsWidget.h"
#include "StatusBar.h"
#include "TabBar.h"
#include "ToolBars.h"
#include "ViewActions.h"
#include "WindowActions.h"

#include <ftk/UI/Divider.h>
#include <ftk/UI/IconSystem.h>
#include <ftk/UI/Menu.h>
#include <ftk/UI/ToolBar.h>

namespace tl
{
    namespace play
    {
        void MainWindow::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app)
        {
            ftk::Window::_init(context, app, "tlplay", ftk::Size2I(1280, 960));

            auto iconSystem = context->getSystem<ftk::IconSystem>();
            setIcon(iconSystem->get("tlRender", 1.0));
            
            // Restore settings.
            bool settingsVisible = false;
            double splitter = 0.8;
            double splitter2 = 0.8;
            auto settings = app->getSettings();
            settings->get("/MainWindow/SettingsVisible", settingsVisible);
            _settingsVisible = ftk::Observable<bool>::create(settingsVisible);
            settings->get("/MainWindow/Splitter", splitter);
            settings->get("/MainWindow/Splitter2", splitter2);
            _settings = settings;

            // Create the viewport.
            _viewport = ui::Viewport::create(context);
            //BackgroundOptions bgOptions;
            //bgOptions.type = Background::Gradient;
            //_viewport->setBackgroundOptions(bgOptions);
            //ForegroundOptions fgOptions;
            //fgOptions.grid.enabled = true;
            //fgOptions.grid.size = 1;
            //fgOptions.grid.labels = GridLabels::Alphanumeric;
            //fgOptions.grid.lineWidth = 10;
            //fgOptions.outline.enabled = true;
            //_viewport->setForegroundOptions(fgOptions);
            //ftk::ImageOptions imageOptions;
            //imageOptions.imageFilters.minify = ftk::ImageFilter::Nearest;
            //imageOptions.imageFilters.magnify = ftk::ImageFilter::Nearest;
            //imageOptions.alphaBlend = ftk::AlphaBlend::None;
            //_viewport->setImageOptions({ imageOptions, imageOptions });
            //DisplayOptions displayOptions;
            //displayOptions.color.enabled = true;
            //displayOptions.color.contrast.x = .1F;
            //displayOptions.color.contrast.y = .1F;
            //displayOptions.color.contrast.z = .1F;
            //displayOptions.exposure.enabled = true;
            //displayOptions.exposure.exposure = 10.F;
            //displayOptions.imageFilters.minify = ftk::ImageFilter::Nearest;
            //displayOptions.imageFilters.magnify = ftk::ImageFilter::Nearest;
            //_viewport->setDisplayOptions({ displayOptions, displayOptions });
            _viewport->setColorBuffer(ftk::gl::TextureType::RGBA_F32);

            // Create the timeline.
            _timelineWidget = ui::TimelineWidget::create(
                context,
                app->getTimeUnitsModel());
            ui::DisplayOptions timelineDisplayOptions;
            timelineDisplayOptions.minimize = false;
            //timelineDisplayOptions.thumbnails = false;
            _timelineWidget->setDisplayOptions(timelineDisplayOptions);
            _timelineWidget->setVStretch(ftk::Stretch::Expanding);

            // Create the actions.
            _fileActions = FileActions::create(context, app);
            _compareActions = CompareActions::create(context, app);
            _playbackActions = PlaybackActions::create(context, app);
            _viewActions = ViewActions::create(context, app, _viewport);
            _windowActions = WindowActions::create(
                context,
                app,
                std::dynamic_pointer_cast<MainWindow>(shared_from_this()));

            // Create the menu bar.
            _menuBar = MenuBar::create(
                context,
                app,
                _fileActions,
                _compareActions,
                _playbackActions,
                _viewActions,
                _windowActions);

            // Create the tool bars.
            auto toolBars = ToolBars::create(
                context,
                _fileActions,
                _compareActions,
                _viewActions,
                _windowActions);
            _playbackBar = PlaybackBar::create(
                context,
                app,
                _playbackActions->getActions());
            _statusBar = StatusBar::create(context, app);

            // Crate the tab bar.
            _tabBar = TabBar::create(context, app);

            // Crate the settings widget.
            _settingsWidget = SettingsWidget::create(context, app);
            _settingsWidget->setVisible(settingsVisible);

            // Layout widgets.
            _layout = ftk::VerticalLayout::create(context, shared_from_this());
            _layout->setSpacingRole(ftk::SizeRole::None);
            _menuBar->setParent(_layout);
            ftk::Divider::create(context, ftk::Orientation::Vertical, _layout);
            toolBars->setParent(_layout);
            ftk::Divider::create(context, ftk::Orientation::Vertical, _layout);
            _splitter = ftk::Splitter::create(context, ftk::Orientation::Vertical, _layout);
            _splitter->setSplit(splitter);
            _splitter2 = ftk::Splitter::create(context, ftk::Orientation::Horizontal, _splitter);
            _splitter2->setSplit(splitter2);
            auto vLayout = ftk::VerticalLayout::create(context, _splitter2);
            vLayout->setSpacingRole(ftk::SizeRole::None);
            _tabBar->setParent(vLayout);
            _viewport->setParent(vLayout);
            _settingsWidget->setParent(_splitter2);
            vLayout = ftk::VerticalLayout::create(context, _splitter);
            vLayout->setSpacingRole(ftk::SizeRole::None);
            _playbackBar->setParent(vLayout);
            ftk::Divider::create(context, ftk::Orientation::Vertical, vLayout);
            _timelineWidget->setParent(vLayout);
            ftk::Divider::create(context, ftk::Orientation::Vertical, vLayout);
            _statusBar->setParent(_layout);

            // Create observers.
            _playerObserver = ftk::Observer<std::shared_ptr<Player> >::create(
                app->getFilesModel()->observePlayer(),
                [this](const std::shared_ptr<Player>& value)
                {
                    _viewport->setPlayer(value);
                    _timelineWidget->setPlayer(value);
                });

            _compareObserver = ftk::Observer<Compare>::create(
                app->getFilesModel()->observeCompare(),
                [this](Compare value)
                {
                    CompareOptions options;
                    options.compare = value;
                    options.overlay = .5F;
                    _viewport->setCompareOptions(options);
                });
        }

        MainWindow::~MainWindow()
        {
            // Save settings.
            if (auto settings = _settings.lock())
            {
                settings->set(
                    "/MainWindow/SettingsVisible",
                    _settingsVisible->get());
                settings->set("/MainWindow/Splitter", _splitter->getSplit());
                settings->set("/MainWindow/Splitter2", _splitter2->getSplit());
            }
        }

        std::shared_ptr<MainWindow> MainWindow::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app)
        {
            auto out = std::shared_ptr<MainWindow>(new MainWindow);
            out->_init(context, app);
            return out;
        }

        const std::shared_ptr<ui::Viewport>& MainWindow::getViewport() const
        {
            return _viewport;
        }

        std::shared_ptr<ftk::IObservable<bool> > MainWindow::observeSettingsVisible() const
        {
            return _settingsVisible;
        }

        void MainWindow::setSettingsVisible(bool value)
        {
            if (_settingsVisible->setIfChanged(value))
            {
                _settingsWidget->setVisible(value);
            }
        }

        void MainWindow::keyPressEvent(ftk::KeyEvent& event)
        {
            event.accept = _menuBar->shortcut(event.key, event.modifiers);
        }

        void MainWindow::keyReleaseEvent(ftk::KeyEvent& event)
        {
            event.accept = true;
        }

        void MainWindow::dropEvent(ftk::DragDropEvent& event)
        {
            event.accept = true;
            if (auto textData = std::dynamic_pointer_cast<ftk::DragDropTextData>(event.data))
            {
                if (auto app = std::dynamic_pointer_cast<App>(getApp()))
                {
                    for (const auto& fileName : textData->getText())
                    {
                        app->open(ftk::Path(fileName));
                    }
                }
            }
        }
    }
}

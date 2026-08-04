// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/UI/TimelineWidget.h>
#include <tlRender/UI/Viewport.h>

#include <tlRender/Timeline/Player.h>

#include <ftk/UI/MainWindow.h>
#include <ftk/UI/Settings.h>
#include <ftk/UI/RowLayout.h>
#include <ftk/UI/Splitter.h>

namespace tl
{
    namespace play
    {
        class App;
        class CompareActions;
        class FileActions;
        class MenuBar;
        class PlaybackActions;
        class PlaybackBar;
        class SettingsModel;
        class SettingsWidget;
        class StatusBar;
        class TabBar;
        class ViewActions;
        class WindowActions;

        //! The main window creates the widgets and actions.
        class MainWindow : public ftk::Window
        {
            FTK_NON_COPYABLE(MainWindow);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<App>&);

            MainWindow() = default;

        public:
            ~MainWindow();

            static std::shared_ptr<MainWindow> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<App>&);

            const std::shared_ptr<ui::Viewport>& getViewport() const;

            std::shared_ptr<ftk::IObservable<bool> > observeSettingsVisible() const;
            void setSettingsVisible(bool);

            void keyPressEvent(ftk::KeyEvent&) override;
            void keyReleaseEvent(ftk::KeyEvent&) override;
            void dropEvent(ftk::DragDropEvent&) override;

        private:
            std::weak_ptr<ftk::Settings> _settings;

            std::shared_ptr<ui::Viewport> _viewport;
            std::shared_ptr<FileActions> _fileActions;
            std::shared_ptr<CompareActions> _compareActions;
            std::shared_ptr<PlaybackActions> _playbackActions;
            std::shared_ptr<ViewActions> _viewActions;
            std::shared_ptr<WindowActions> _windowActions;
            std::shared_ptr<MenuBar> _menuBar;
            std::shared_ptr<TabBar> _tabBar;
            std::shared_ptr<PlaybackBar> _playbackBar;
            std::shared_ptr<ui::TimelineWidget> _timelineWidget;
            std::shared_ptr<StatusBar> _statusBar;
            std::shared_ptr<SettingsWidget> _settingsWidget;
            std::shared_ptr<ftk::Observable<bool> > _settingsVisible;
            std::shared_ptr<ftk::Splitter> _splitter;
            std::shared_ptr<ftk::Splitter> _splitter2;
            std::shared_ptr<ftk::VerticalLayout> _layout;

            std::shared_ptr<ftk::Observer<std::shared_ptr<Player> > > _playerObserver;
            std::shared_ptr<ftk::Observer<Compare> > _compareObserver;
        };
    }
}

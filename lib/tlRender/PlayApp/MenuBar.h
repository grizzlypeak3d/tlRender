// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Player.h>

#include <ftk/UI/ActionGroup.h>
#include <ftk/UI/Menu.h>
#include <ftk/UI/MenuBar.h>

namespace tl
{
    namespace play
    {
        class App;
        class CompareActions;
        class FileActions;
        class WindowActions;
        class ViewActions;
        class PlaybackActions;

        //! File menu.
        class FileMenu : public ftk::Menu
        {
            FTK_NON_COPYABLE(FileMenu);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<App>&,
                const std::shared_ptr<FileActions>&,
                const std::shared_ptr<IWidget>& parent);

            FileMenu() = default;

        public:
            ~FileMenu();

            static std::shared_ptr<FileMenu> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<App>&,
                const std::shared_ptr<FileActions>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

        private:
            int _playerIndex = -1;
            std::vector<std::shared_ptr<ftk::Action> > _activeActions;

            //! Rebuilt whenever the files change, so the group is cleared and
            //! refilled rather than made once.
            std::shared_ptr<ftk::ActionGroup> _activeGroup;
            std::shared_ptr<ftk::Menu> _activeMenu;
            std::vector<std::shared_ptr<ftk::Action> > _recentActions;
            std::shared_ptr<ftk::Menu> _recentMenu;
            std::shared_ptr<ftk::ListObserver<std::shared_ptr<Player> > > _playersObserver;
            std::shared_ptr<ftk::Observer<int> > _playerIndexObserver;
            std::shared_ptr<ftk::ListObserver<std::filesystem::path> > _recentObserver;
        };

        //! Compare menu.
        class CompareMenu : public ftk::Menu
        {
            FTK_NON_COPYABLE(CompareMenu);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<App>&,
                const std::shared_ptr<CompareActions>&,
                const std::shared_ptr<IWidget>& parent);

            CompareMenu() = default;

        public:
            ~CompareMenu();

            static std::shared_ptr<CompareMenu> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<App>&,
                const std::shared_ptr<CompareActions>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

        private:
            std::vector<std::shared_ptr<ftk::Action> > _bFileActions;

            //! Toggle rather than Radio: turning the current B file off means
            //! there is no B file, which is a thing that can be.
            std::shared_ptr<ftk::ActionGroup> _bFileGroup;
            std::shared_ptr<ftk::Menu> _bFileMenu;
            int _bPlayerIndex = -1;
            std::shared_ptr<ftk::ListObserver<std::shared_ptr<Player> > > _playersObserver;
            std::shared_ptr<ftk::Observer<int> > _bPlayerIndexObserver;
        };

        //! Playback menu.
        class PlaybackMenu : public ftk::Menu
        {
            FTK_NON_COPYABLE(PlaybackMenu);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<PlaybackActions>&,
                const std::shared_ptr<IWidget>& parent);

            PlaybackMenu() = default;

        public:
            ~PlaybackMenu();

            static std::shared_ptr<PlaybackMenu> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<PlaybackActions>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

        private:
        };

        //! View menu.
        class ViewMenu : public ftk::Menu
        {
            FTK_NON_COPYABLE(ViewMenu);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<ViewActions>&,
                const std::shared_ptr<IWidget>& parent);

            ViewMenu() = default;

        public:
            ~ViewMenu();

            static std::shared_ptr<ViewMenu> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<ViewActions>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

        private:
        };

        //! Window menu.
        class WindowMenu : public ftk::Menu
        {
            FTK_NON_COPYABLE(WindowMenu);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<WindowActions>&,
                const std::shared_ptr<IWidget>& parent);

            WindowMenu() = default;

        public:
            ~WindowMenu();

            static std::shared_ptr<WindowMenu> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<WindowActions>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

        private:
        };

        //! Menu bar.
        class MenuBar : public ftk::MenuBar
        {
            FTK_NON_COPYABLE(MenuBar);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<App>&,
                const std::shared_ptr<FileActions>&,
                const std::shared_ptr<CompareActions>&,
                const std::shared_ptr<PlaybackActions>&,
                const std::shared_ptr<ViewActions>&,
                const std::shared_ptr<WindowActions>&,
                const std::shared_ptr<IWidget>& parent);

            MenuBar() = default;

        public:
            ~MenuBar();

            static std::shared_ptr<MenuBar> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<App>&,
                const std::shared_ptr<FileActions>&,
                const std::shared_ptr<CompareActions>&,
                const std::shared_ptr<PlaybackActions>&,
                const std::shared_ptr<ViewActions>&,
                const std::shared_ptr<WindowActions>&,
                const std::shared_ptr<IWidget>& parent = nullptr);
        };
    }
}

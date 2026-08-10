// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Player.h>

#include <ftk/UI/ActionGroup.h>

namespace tl
{
    namespace play
    {
        class App;

        //! This class provides compare actions.
        class PlaybackActions : public std::enable_shared_from_this<PlaybackActions>
        {
            FTK_NON_COPYABLE(PlaybackActions);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<App>&);

            PlaybackActions() = default;

        public:
            ~PlaybackActions();

            static std::shared_ptr<PlaybackActions> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<App>&);

            const std::map<std::string, std::shared_ptr<ftk::Action> >& getActions() const;

        private:
            std::map<std::string, std::shared_ptr<ftk::Action> > _actions;
            std::shared_ptr<ftk::ActionGroup> _playbackGroup;
            std::shared_ptr<Player> _player;
            std::shared_ptr<ftk::Observer<std::shared_ptr<Player> > > _playerObserver;
            std::shared_ptr<ftk::Observer<Playback> > _playbackObserver;
        };
    }
}

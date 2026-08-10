// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include "PlaybackActions.h"

#include "App.h"
#include "FilesModel.h"

namespace tl
{
    namespace play
    {
        void PlaybackActions::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app)
        {
            _actions["Stop"] = ftk::Action::create(
                "Stop",
                "PlaybackStop",
                ftk::Key::K,
                [this]
                {
                    if (_player)
                    {
                        _player->stop();
                    }
                });
            _actions["Stop"]->setTooltip("Stop playback.");

            _actions["Forward"] = ftk::Action::create(
                "Forward",
                "PlaybackForward",
                ftk::Key::L,
                [this]
                {
                    if (_player)
                    {
                        _player->forward();
                    }
                });
            _actions["Forward"]->setTooltip("Start forward playback.");

            _actions["Reverse"] = ftk::Action::create(
                "Reverse",
                "PlaybackReverse",
                ftk::Key::J,
                [this]
                {
                    if (_player)
                    {
                        _player->reverse();
                    }
                });
            _actions["Reverse"]->setTooltip("Start reverse playback.");

            _actions["TogglePlayback"] = ftk::Action::create(
                "Toggle Playback",
                ftk::Key::Space,
                [this]
                {
                    if (_player)
                    {
                        _player->togglePlayback();
                    }
                });

            _actions["Start"] = ftk::Action::create(
                "Start Frame",
                "FrameStart",
                ftk::Key::Home,
                [this]
                {
                    if (_player)
                    {
                        _player->gotoStart();
                    }
                });
            _actions["Start"]->setTooltip("Go to the start frame.");

            _actions["Prev"] = ftk::Action::create(
                "Previous Frame",
                "FramePrev",
                ftk::Key::Left,
                [this]
                {
                    if (_player)
                    {
                        _player->framePrev();
                    }
                });
            _actions["Prev"]->setTooltip("Go to the previous frame.");

            _actions["Next"] = ftk::Action::create(
                "Next Frame",
                "FrameNext",
                ftk::Key::Right,
                [this]
                {
                    if (_player)
                    {
                        _player->frameNext();
                    }
                });
            _actions["Next"]->setTooltip("Go to the next frame.");

            _actions["End"] = ftk::Action::create(
                "End Frame",
                "FrameEnd",
                ftk::Key::End,
                [this]
                {
                    if (_player)
                    {
                        _player->gotoEnd();
                    }
                });
            _actions["End"]->setTooltip("Go to the end frame.");

            _actions["JumpBack1s"] = ftk::Action::create(
                "Jump Back 1s",
                ftk::KeyShortcut(ftk::Key::J, static_cast<int>(ftk::KeyModifier::Shift)),
                [this]
                {
                    if (_player)
                    {
                        _player->timeAction(TimeAction::JumpBack1s);
                    }
                });
            _actions["JumpBack1s"]->setTooltip("Jump backwards one second.");

            _actions["JumpForward1s"] = ftk::Action::create(
                "Jump Forward 1s",
                ftk::KeyShortcut(ftk::Key::L, static_cast<int>(ftk::KeyModifier::Shift)),
                [this]
                {
                    if (_player)
                    {
                        _player->timeAction(TimeAction::JumpForward1s);
                    }
                });
            _actions["JumpForward1s"]->setTooltip("Jump forwards one second.");

            _actions["SetInPoint"] = ftk::Action::create(
                "Set In Point",
                ftk::Key::I,
                [this]
                {
                    if (_player)
                    {
                        _player->setInPoint();
                    }
                });
            _actions["SetInPoint"]->setTooltip("Set the playback in point.");

            _actions["ResetInPoint"] = ftk::Action::create(
                "Reset In Point",
                ftk::KeyShortcut(ftk::Key::I, static_cast<int>(ftk::KeyModifier::Shift)),
                [this]
                {
                    if (_player)
                    {
                        _player->resetInPoint();
                    }
                });
            _actions["ResetInPoint"]->setTooltip("Reset the playback in point.");

            _actions["SetOutPoint"] = ftk::Action::create(
                "Set Out Point",
                ftk::Key::O,
                [this]
                {
                    if (_player)
                    {
                        _player->setOutPoint();
                    }
                });
            _actions["SetOutPoint"]->setTooltip("Set the playback out point.");

            _actions["ResetOutPoint"] = ftk::Action::create(
                "Reset Out Point",
                ftk::KeyShortcut(ftk::Key::O, static_cast<int>(ftk::KeyModifier::Shift)),
                [this]
                {
                    if (_player)
                    {
                        _player->resetOutPoint();
                    }
                });
            _actions["ResetOutPoint"]->setTooltip("Reset the playback out point.");

            _playbackGroup = ftk::ActionGroup::create(ftk::ActionGroupType::Radio);
            _playbackGroup->addAction(_actions["Stop"]);
            _playbackGroup->addAction(_actions["Forward"]);
            _playbackGroup->addAction(_actions["Reverse"]);

            _playerObserver = ftk::Observer<std::shared_ptr<Player> >::create(
                app->getFilesModel()->observePlayer(),
                [this](const std::shared_ptr<Player>& value)
                {
                    _player = value;

                    if (value)
                    {
                        _playbackObserver = ftk::Observer<Playback>::create(
                            value->observePlayback(),
                            [this](Playback value)
                            {
                                _playbackGroup->setChecked(
                                    static_cast<int>(value));
                            });
                    }
                    else
                    {
                        _playbackObserver.reset();

                        _playbackGroup->setChecked(
                            static_cast<int>(Playback::Stop));
                    }

                    _actions["Stop"]->setEnabled(value.get());
                    _actions["Forward"]->setEnabled(value.get());
                    _actions["Reverse"]->setEnabled(value.get());
                    _actions["TogglePlayback"]->setEnabled(value.get());
                    _actions["Start"]->setEnabled(value.get());
                    _actions["Prev"]->setEnabled(value.get());
                    _actions["Next"]->setEnabled(value.get());
                    _actions["End"]->setEnabled(value.get());
                    _actions["SetInPoint"]->setEnabled(value.get());
                    _actions["ResetInPoint"]->setEnabled(value.get());
                    _actions["SetOutPoint"]->setEnabled(value.get());
                    _actions["ResetOutPoint"]->setEnabled(value.get());
                });
        }

        PlaybackActions::~PlaybackActions()
        {}

        std::shared_ptr<PlaybackActions> PlaybackActions::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app)
        {
            auto out = std::shared_ptr<PlaybackActions>(new PlaybackActions);
            out->_init(context, app);
            return out;
        }

        const std::map<std::string, std::shared_ptr<ftk::Action> >& PlaybackActions::getActions() const
        {
            return _actions;
        }
    }
}

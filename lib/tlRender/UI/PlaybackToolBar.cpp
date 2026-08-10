// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UI/PlaybackToolBar.h>

#include <tlRender/UI/PlaybackLoopWidget.h>

#include <tlRender/Timeline/Player.h>

#include <ftk/UI/ActionGroup.h>

namespace tl
{
    namespace ui
    {
        struct PlaybackToolBar::Private
        {
            std::map<std::string, std::shared_ptr<ftk::Action> > actions;
            std::shared_ptr<PlaybackLoopWidget> loopWidget;
            std::shared_ptr<Player> player;
            std::shared_ptr<ftk::Observer<Loop> > loopObserver;
            //! Stop, forward and reverse are one of three: a player is always
            //! doing exactly one of them.
            std::shared_ptr<ftk::ActionGroup> playbackGroup;
            std::shared_ptr<ftk::Observer<Playback> > playbackObserver;
        };

        void PlaybackToolBar::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<IWidget>& parent)
        {
            ToolBar::_init(context, ftk::Orientation::Horizontal, parent);
            FTK_P();

            p.actions["Stop"] = ftk::Action::create(
                "Stop",
                "PlaybackStop",
                ftk::Key::K,
                [this]
                {
                    if (_p->player)
                    {
                        _p->player->stop();
                    }
                });
            p.actions["Stop"]->setTooltip("Stop playback.");

            p.actions["Forward"] = ftk::Action::create(
                "Forward",
                "PlaybackForward",
                ftk::Key::L,
                [this]
                {
                    if (_p->player)
                    {
                        _p->player->forward();
                    }
                });
            p.actions["Forward"]->setTooltip("Start forward playback.");

            p.actions["Reverse"] = ftk::Action::create(
                "Reverse",
                "PlaybackReverse",
                ftk::Key::J,
                [this]
                {
                    if (_p->player)
                    {
                        _p->player->reverse();
                    }
                });
            p.actions["Reverse"]->setTooltip("Start reverse playback.");

            p.loopWidget = PlaybackLoopWidget::create(context);
            p.loopWidget->setTooltip("Playback loop mode.");

            // Added in the enumeration's order rather than the tool bar's, so
            // that the group's index is the Playback value.
            p.playbackGroup = ftk::ActionGroup::create(ftk::ActionGroupType::Radio);
            p.playbackGroup->addAction(p.actions["Stop"]);
            p.playbackGroup->addAction(p.actions["Forward"]);
            p.playbackGroup->addAction(p.actions["Reverse"]);

            addAction(p.actions["Reverse"]);
            addAction(p.actions["Stop"]);
            addAction(p.actions["Forward"]);
            addWidget(p.loopWidget);

            _widgetUpdate();

            p.loopWidget->setCallback(
                [this](Loop value)
                {
                    if (_p->player)
                    {
                        _p->player->setLoop(value);
                    }
                });
        }

        PlaybackToolBar::PlaybackToolBar() :
            _p(new Private)
        {}

        PlaybackToolBar::~PlaybackToolBar()
        {}

        std::shared_ptr<PlaybackToolBar> PlaybackToolBar::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<IWidget>& parent)
        {
            auto out = std::shared_ptr<PlaybackToolBar>(new PlaybackToolBar);
            out->_init(context, parent);
            return out;
        }
            
        const std::map<std::string, std::shared_ptr<ftk::Action> >& PlaybackToolBar::getActions() const
        {
            return _p->actions;
        }

        const std::shared_ptr<Player>& PlaybackToolBar::getPlayer() const
        {
            return _p->player;
        }

        void PlaybackToolBar::setPlayer(const std::shared_ptr<Player>& player)
        {
            FTK_P();
            p.player = player;
            if (player)
            {
                p.loopObserver = ftk::Observer<Loop>::create(
                    player->observeLoop(),
                    [this](Loop value)
                    {
                        _p->loopWidget->setLoop(value);
                    });

                p.playbackObserver = ftk::Observer<Playback>::create(
                    player->observePlayback(),
                    [this](Playback value)
                    {
                        _p->playbackGroup->setChecked(static_cast<int>(value));
                    });
            }
            else
            {
                p.loopObserver.reset();
                p.playbackObserver.reset();
            }
            _widgetUpdate();
        }

        void PlaybackToolBar::_widgetUpdate()
        {
            FTK_P();
            if (!p.player)
            {
                p.actions["Stop"]->setChecked(true);
                p.actions["Forward"]->setChecked(false);
                p.actions["Reverse"]->setChecked(false);
                p.loopWidget->setLoop(Loop::Loop);
            }
            p.actions["Stop"]->setEnabled(p.player.get());
            p.actions["Forward"]->setEnabled(p.player.get());
            p.actions["Reverse"]->setEnabled(p.player.get());
            p.loopWidget->setEnabled(p.player.get());
        }
    }
}

// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include "PlaybackBar.h"

#include "App.h"
#include "FilesModel.h"

#include <ftk/UI/ToolButton.h>
#include <ftk/Core/Format.h>

namespace tl
{
    namespace play
    {
        void PlaybackBar::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app,
            const std::map<std::string, std::shared_ptr<ftk::Action> >& actions,
            const std::shared_ptr<IWidget>& parent)
        {
            IWidget::_init(context, "PlaybackBar", parent);

            _layout = ftk::HorizontalLayout::create(context, shared_from_this());
            _layout->setMarginRole(ftk::SizeRole::MarginInside);

            auto hLayout = ftk::HorizontalLayout::create(context, _layout);
            hLayout->setSpacingRole(ftk::SizeRole::None);
            auto tmp = actions;
            auto reverseButton = ftk::ToolButton::create(context, tmp["Reverse"], hLayout);
            auto stopButton = ftk::ToolButton::create(context, tmp["Stop"], hLayout);
            auto forwardButton = ftk::ToolButton::create(context, tmp["Forward"], hLayout);
            _loopWidget = ui::PlaybackLoopWidget::create(context, hLayout);

            hLayout = ftk::HorizontalLayout::create(context, _layout);
            hLayout->setSpacingRole(ftk::SizeRole::None);
            auto startButton = ftk::ToolButton::create(context, tmp["Start"], hLayout);
            auto prevButton = ftk::ToolButton::create(context, tmp["Prev"], hLayout);
            prevButton->setRepeatClick(true);
            auto nextButton = ftk::ToolButton::create(context, tmp["Next"], hLayout);
            nextButton->setRepeatClick(true);
            auto endButton = ftk::ToolButton::create(context, tmp["End"], hLayout);

            auto timeUnitsModel = app->getTimeUnitsModel();
            _currentTimeEdit = ui::TimeEdit::create(context, timeUnitsModel, _layout);
            _currentTimeEdit->setTooltip("The current time.");

            _durationLabel = ui::TimeLabel::create(context, timeUnitsModel, _layout);
            _durationLabel->setTooltip("The timeline duration.");

            _speedEdit = ftk::DoubleEdit::create(context, _layout);
            _speedEdit->setRange(ftk::RangeD(1.0, 99999.0));
            _speedEdit->setStep(1.0);
            _speedEdit->setLargeStep(10.0);
            _speedEdit->setTooltip("Set the timeline speed.");

            _actualSpeedLabel = ftk::Label::create(context, _layout);
            _actualSpeedLabel->setHMarginRole(ftk::SizeRole::MarginInside);
            _actualSpeedLabel->setTooltip("Actual playback speed.");

            _timeUnitsWidget = ui::TimeUnitsWidget::create(context, timeUnitsModel, _layout);
            _timeUnitsWidget->setTooltip("Set the time units.");

            _loopWidget->setCallback(
                [this](Loop value)
                {
                    if (_player)
                    {
                        _player->setLoop(value);
                    }
                });

            _currentTimeEdit->setCallback(
                [this](const OTIO_NS::RationalTime& value)
                {
                    if (_player)
                    {
                        _player->stop();
                        _player->seek(value);
                    }
                });

            _speedEdit->setCallback(
                [this](double value)
                {
                    if (_player)
                    {
                        _player->setSpeed(value);
                    }
                });

            _playerObserver = ftk::Observer<std::shared_ptr<Player> >::create(
                app->getFilesModel()->observePlayer(),
                [this](const std::shared_ptr<Player>& value)
                {
                    _player = value;

                    if (value)
                    {
                        _durationLabel->setValue(value->getTimeRange().duration());

                        _loopObserver = ftk::Observer<Loop>::create(
                            value->observeLoop(),
                            [this](Loop value)
                            {
                                _loopWidget->setLoop(value);
                            });

                        _currentTimeObserver = ftk::Observer<OTIO_NS::RationalTime>::create(
                            value->observeCurrentTime(),
                            [this](const OTIO_NS::RationalTime& value)
                            {
                                _currentTimeEdit->setValue(value);
                            });

                        _speedObserver = ftk::Observer<double>::create(
                            value->observeSpeed(),
                            [this](double value)
                            {
                                _speedEdit->setValue(value);
                            });

                        _actualSpeedObserver = ftk::Observer<double>::create(
                            value->observeActualSpeed(),
                            [this](double value)
                            {
                                _actualSpeedLabel->setText(ftk::Format("{0}").arg(value));
                            });
                    }
                    else
                    {
                        _loopWidget->setLoop(Loop::Loop);
                        _currentTimeEdit->setValue(std::nullopt);
                        _durationLabel->setValue(std::nullopt);
                        _actualSpeedLabel->setText("0");

                        _loopObserver.reset();
                        _currentTimeObserver.reset();
                        _speedObserver.reset();
                        _actualSpeedObserver.reset();
                    }

                    _loopWidget->setEnabled(value.get());
                    _currentTimeEdit->setEnabled(value.get());
                    _durationLabel->setEnabled(value.get());
                    _speedEdit->setEnabled(value.get());
                    _actualSpeedLabel->setEnabled(value.get());
                });
        }

        PlaybackBar::~PlaybackBar()
        {}

        std::shared_ptr<PlaybackBar> PlaybackBar::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app,
            const std::map<std::string, std::shared_ptr<ftk::Action> >& actions,
            const std::shared_ptr<IWidget>& parent)
        {
            auto out = std::shared_ptr<PlaybackBar>(new PlaybackBar);
            out->_init(context, app, actions, parent);
            return out;
        }

        ftk::Size2I PlaybackBar::getSizeHint() const
        {
            return _layout->getSizeHint();
        }

        void PlaybackBar::setGeometry(const ftk::Box2I& value)
        {
            IWidget::setGeometry(value);
            _layout->setGeometry(value);
        }
    }
}
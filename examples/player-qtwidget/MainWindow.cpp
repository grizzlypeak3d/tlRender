// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include "MainWindow.h"

#include <QDockWidget>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>

namespace tl
{
    namespace examples
    {
        namespace player_qtwidget
        {
            MainWindow::MainWindow(const std::shared_ptr<ftk::Context>& context)
            {
                // Create the models.
                auto timeUnitsModel = TimeUnitsModel::create(context);
                auto timeObject = new qt::TimeObject(timeUnitsModel, this);
                auto style = ftk::Style::create(context);

                // Create the viewport.
                _viewport = new qtwidget::Viewport(context, style);
                setCentralWidget(_viewport);

                // Create the timeline widget.
                _timelineWidget = new qtwidget::TimelineWidget(context, timeUnitsModel, style);
                auto timelineDockWidget = new QDockWidget;
                timelineDockWidget->setFeatures(QDockWidget::NoDockWidgetFeatures);
                timelineDockWidget->setTitleBarWidget(new QWidget);
                timelineDockWidget->setWidget(_timelineWidget);
                addDockWidget(Qt::BottomDockWidgetArea, timelineDockWidget);

                // Create the tool bar.
                _playbackAction = new QAction(this);
                _playbackAction->setIcon(this->style()->standardIcon(QStyle::SP_MediaPlay));
                _playbackAction->setCheckable(true);
                _playbackAction->setToolTip(tr("Toggle playback"));
                _currentTimeSpinBox = new qtwidget::TimeSpinBox;
                _currentTimeSpinBox->setTimeObject(timeObject);
                _currentTimeSpinBox->setToolTip("Current time");
                _durationLabel = new qtwidget::TimeLabel;
                _durationLabel->setTimeObject(timeObject);
                _durationLabel->setToolTip("Duration");
                auto toolBar = new QToolBar;
                toolBar->setFloatable(false);
                toolBar->setMovable(false);
                toolBar->addAction(_playbackAction);
                toolBar->addWidget(_currentTimeSpinBox);
                toolBar->addWidget(_durationLabel);
                addToolBar(Qt::BottomToolBarArea, toolBar);

                // Update the widget.
                _currentTimeUpdate(std::nullopt);
                _durationUpdate(std::nullopt);
                _playbackUpdate(Playback::Stop);

                // Setup connections.
                connect(
                    _playbackAction,
                    &QAction::toggled,
                    [this](bool value)
                    {
                        if (_player)
                        {
                            const Playback playback = value ?
                                Playback::Forward :
                                Playback::Stop;
                            _player->setPlayback(playback);
                            _playbackUpdate(playback);
                        }
                    });

                connect(
                    _currentTimeSpinBox,
                    &qtwidget::TimeSpinBox::valueChanged,
                    [this](const OTIO_NS::RationalTime& value)
                    {
                        if (_player)
                        {
                            _player->stop();
                            _player->seek(value);
                        }
                    });
            }

            MainWindow::~MainWindow()
            {}

            void MainWindow::setPlayer(const QSharedPointer<qt::PlayerObject>& player)
            {
                disconnect(_currentTimeConnection);
                disconnect(_playbackConnection);

                _player = player;
                _viewport->setPlayer(player);
                _timelineWidget->setPlayer(player ? player->player() : nullptr);

                _currentTimeUpdate(player ?
                    std::optional<OTIO_NS::RationalTime>(player->currentTime()) :
                    std::nullopt);
                _durationUpdate(player ?
                    std::optional<OTIO_NS::RationalTime>(player->timeRange().duration()) :
                    std::nullopt);
                _playbackUpdate(player ? player->playback() : Playback::Stop);

                if (player)
                {
                    _currentTimeConnection = connect(
                        player.get(),
                        &qt::PlayerObject::currentTimeChanged,
                        [this](const OTIO_NS::RationalTime& value)
                        {
                            _currentTimeUpdate(value);
                        });
                    _playbackConnection = connect(
                        player.get(),
                        &qt::PlayerObject::playbackChanged,
                        [this](Playback value)
                        {
                            _playbackUpdate(value);
                        });
                }
            }

            void MainWindow::_currentTimeUpdate(const std::optional<OTIO_NS::RationalTime>& value)
            {
                QSignalBlocker blocker(_currentTimeSpinBox);
                _currentTimeSpinBox->setValue(value);
            }

            void MainWindow::_durationUpdate(const std::optional<OTIO_NS::RationalTime>& value)
            {
                _durationLabel->setValue(value);
            }

            void MainWindow::_playbackUpdate(Playback playback)
            {
                QSignalBlocker blocker(_playbackAction);
                _playbackAction->setChecked(Playback::Forward == playback);
            }
        }
    }
}

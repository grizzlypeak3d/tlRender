// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/QtWidget/TimelineWidget.h>

#include <tlRender/QtWidget/Util.h>

#include <tlRender/UI/TimelineWidget.h>
#include <tlRender/UI/TimelineWidget.h>

#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>

namespace tl
{
    namespace qtwidget
    {
        struct TimelineWidget::Private
        {
            std::shared_ptr<ui::TimelineWidget> timelineWidget;

            std::shared_ptr<ftk::Observer<bool> > frameViewObserver;
            std::shared_ptr<ftk::Observer<bool> > scrubObserver;
            std::shared_ptr<ftk::Observer<std::optional<OTIO_NS::RationalTime> > > timeScrubObserver;
        };

        TimelineWidget::TimelineWidget(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<ITimeUnitsModel>& timeUnitsModel,
            const std::shared_ptr<ftk::Style>& style,
            QWidget* parent) :
            ContainerWidget(context, style, parent),
            _p(new Private)
        {
            FTK_P();

            p.timelineWidget = ui::TimelineWidget::create(context, timeUnitsModel);
            setWidget(p.timelineWidget);

            p.frameViewObserver = ftk::Observer<bool>::create(
                p.timelineWidget->observeFrameView(),
                [this](bool value)
                {
                    Q_EMIT frameViewChanged(value);
                });

            p.scrubObserver = ftk::Observer<bool>::create(
                p.timelineWidget->observeScrub(),
                [this](bool value)
                {
                    Q_EMIT scrubChanged(value);
                });

            p.timeScrubObserver = ftk::Observer<std::optional<OTIO_NS::RationalTime> >::create(
                p.timelineWidget->observeTimeScrub(),
                [this](const std::optional<OTIO_NS::RationalTime>& value)
                {
                    // Nothing has been scrubbed to yet, so there is no
                    // position to report.
                    if (value.has_value())
                    {
                        Q_EMIT timeScrubbed(*value);
                    }
                });
        }

        TimelineWidget::~TimelineWidget()
        {}

        std::shared_ptr<Player>& TimelineWidget::player() const
        {
            return _p->timelineWidget->getPlayer();
        }

        void TimelineWidget::setPlayer(const std::shared_ptr<Player>& player)
        {
            _p->timelineWidget->setPlayer(player);
        }

        bool TimelineWidget::hasFrameView() const
        {
            return _p->timelineWidget->hasFrameView();
        }

        bool TimelineWidget::areScrollBarsVisible() const
        {
            return _p->timelineWidget->areScrollBarsVisible();
        }

        bool TimelineWidget::hasAutoScroll() const
        {
            return _p->timelineWidget->hasAutoScroll();
        }

        bool TimelineWidget::hasStopOnScrub() const
        {
            return _p->timelineWidget->hasStopOnScrub();
        }

        const std::vector<int>& TimelineWidget::frameMarkers() const
        {
            return _p->timelineWidget->getFrameMarkers();
        }

        const ui::ItemOptions& TimelineWidget::itemOptions() const
        {
            return _p->timelineWidget->getItemOptions();
        }

        const ui::DisplayOptions& TimelineWidget::displayOptions() const
        {
            return _p->timelineWidget->getDisplayOptions();
        }

        void TimelineWidget::setFrameView(bool value)
        {
            _p->timelineWidget->setFrameView(value);
        }

        void TimelineWidget::setScrollBarsVisible(bool value)
        {
            _p->timelineWidget->setScrollBarsVisible(value);
        }

        void TimelineWidget::setAutoScroll(bool value)
        {
            _p->timelineWidget->setAutoScroll(value);
        }

        void TimelineWidget::setScrollBinding(ftk::MouseButton button, ftk::KeyModifier modifier)
        {
            _p->timelineWidget->setScrollBinding(button, modifier);
        }

        void TimelineWidget::setMouseWheelScale(float value)
        {
            _p->timelineWidget->setMouseWheelScale(value);
        }

        void TimelineWidget::setStopOnScrub(bool value)
        {
            _p->timelineWidget->setStopOnScrub(value);
        }

        void TimelineWidget::setFrameMarkers(const std::vector<int>& value)
        {
            _p->timelineWidget->setFrameMarkers(value);
        }

        void TimelineWidget::setItemOptions(const ui::ItemOptions& value)
        {
            FTK_P();
            p.timelineWidget->setItemOptions(value);
            setInputEnabled(value.inputEnabled);
        }

        void TimelineWidget::setDisplayOptions(const ui::DisplayOptions& value)
        {
            _p->timelineWidget->setDisplayOptions(value);
        }
    }
}

// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UI/TimelineWidget.h>

#include <tlRender/UI/TimelineRuler.h>

#include <tlRender/Timeline/TimeUnits.h>

#include <ftk/UI/RowLayout.h>
#include <ftk/UI/ScrollWidget.h>

#include <algorithm>

namespace tl
{
    namespace ui
    {
        namespace
        {
            const float marginPercentage = .1F;
        }

        struct TimelineWidget::Private
        {
            std::shared_ptr<ITimeUnitsModel> timeUnitsModel;

            //! One per item: the media time conversion is bound to a
            //! particular timeline, so it cannot be shared between them.
            std::vector<std::shared_ptr<ItemData> > itemData;

            std::shared_ptr<Player> player;
            std::vector<std::shared_ptr<Timeline> > compare;
            std::shared_ptr<ftk::Observable<bool> > frameView;
            std::shared_ptr<ftk::Observable<bool> > scrollBarsVisible;
            std::shared_ptr<ftk::Observable<bool> > autoScroll;
            std::pair<ftk::MouseButton, ftk::KeyModifier> scrollBinding =
                std::make_pair(ftk::MouseButton::Middle, ftk::KeyModifier::None);
            float mouseWheelScale = 1.1F;
            std::shared_ptr<ftk::Observable<bool> > stopOnScrub;
            std::shared_ptr<ftk::Observable<bool> > scrub;
            std::shared_ptr<ftk::Observable<std::optional<OTIO_NS::RationalTime> > > timeScrub;
            std::vector<int> frameMarkers;
            std::vector<ItemColors> itemColors;
            std::vector<std::string> labels;
            std::shared_ptr<ftk::Observable<ItemOptions> > itemOptions;
            std::shared_ptr<ftk::Observable<DisplayOptions> > displayOptions;
            OTIO_NS::TimeRange timeRange;
            Playback playback = Playback::Stop;
            OTIO_NS::RationalTime currentTime;
            double scale = 500.0;
            bool sizeInit = true;
            float displayScale = 0.F;
            int border = 0;

            //! One scroll area for every timeline, so that they share a scroll
            //! position rather than being kept in step with each other. The
            //! first item is the player's; the rest are what it is being
            //! compared against.
            std::shared_ptr<TimelineRuler> ruler;
            std::shared_ptr<ftk::ScrollWidget> scrollWidget;
            std::shared_ptr<ftk::VerticalLayout> layout;
            std::vector<std::shared_ptr<TimelineItem> > timelineItems;

            enum class MouseMode
            {
                None,
                Scroll
            };
            struct MouseData
            {
                bool inside = false;
                ftk::V2I press;
                MouseMode mode = MouseMode::None;
                ftk::V2I scrollPos;
            };
            MouseData mouse;

            std::shared_ptr<ftk::Observer<Playback> > playbackObserver;
            std::shared_ptr<ftk::Observer<OTIO_NS::RationalTime> > currentTimeObserver;
            std::shared_ptr<ftk::Observer<std::string> > mediaReferenceKeyObserver;
            std::shared_ptr<ftk::ListObserver<std::shared_ptr<Timeline> > > compareObserver;
            std::shared_ptr<ftk::Observer<bool> > scrubObserver;
            std::shared_ptr<ftk::Observer<std::optional<OTIO_NS::RationalTime> > > timeScrubObserver;
        };

        void TimelineWidget::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<ITimeUnitsModel>& timeUnitsModel,
            const std::shared_ptr<IWidget>& parent)
        {
            IWidget::_init(context, "tl::ui::TimelineWidget", parent);
            FTK_P();

            p.timeUnitsModel = timeUnitsModel ?
                timeUnitsModel :
                TimeUnitsModel::create(context);

            p.frameView = ftk::Observable<bool>::create(true);
            p.scrollBarsVisible = ftk::Observable<bool>::create(true);
            p.autoScroll = ftk::Observable<bool>::create(true);
            p.stopOnScrub = ftk::Observable<bool>::create(true);
            p.scrub = ftk::Observable<bool>::create(false);
            p.timeScrub = ftk::Observable<std::optional<OTIO_NS::RationalTime> >::create();
            p.itemOptions = ftk::Observable<ItemOptions>::create();
            p.displayOptions = ftk::Observable<DisplayOptions>::create();

            // The ruler is outside the scroll area so that it stays put while
            // the timelines scroll under it, and follows their scale and
            // scroll position instead. Laid out by hand rather than stacked
            // in a layout: it has to end where the scrolled timelines end,
            // and a vertical scroll bar takes width from them and not from a
            // sibling above them.
            p.ruler = TimelineRuler::create(context, nullptr, shared_from_this());

            p.scrollWidget = ftk::ScrollWidget::create(
                context,
                ftk::ScrollType::Both,
                shared_from_this());
            p.scrollWidget->setScrollBarsVisible(p.scrollBarsVisible->get());
            p.scrollWidget->setScrollEventsEnabled(false);
            p.scrollWidget->setBorder(false);

            p.layout = ftk::VerticalLayout::create(context);
            p.layout->setSpacingRole(ftk::SizeRole::None);
            p.scrollWidget->setWidget(p.layout);

            p.scrollWidget->setScrollPosCallback(
                [this](const ftk::V2I& value)
                {
                    _p->ruler->setScrollPos(value.x);
                });
        }

        TimelineWidget::TimelineWidget() :
            _p(new Private)
        {}

        TimelineWidget::~TimelineWidget()
        {}

        std::shared_ptr<TimelineWidget> TimelineWidget::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<IWidget>& parent)
        {
            auto out = std::shared_ptr<TimelineWidget>(new TimelineWidget);
            out->_init(context, nullptr, parent);
            return out;
        }

        std::shared_ptr<TimelineWidget> TimelineWidget::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<ITimeUnitsModel>& timeUnitsModel,
            const std::shared_ptr<IWidget>& parent)
        {
            auto out = std::shared_ptr<TimelineWidget>(new TimelineWidget);
            out->_init(context, timeUnitsModel, parent);
            return out;
        }

        const std::shared_ptr<ITimeUnitsModel>& TimelineWidget::getTimeUnitsModel() const
        {
            return _p->timeUnitsModel;
        }

        std::shared_ptr<Player>& TimelineWidget::getPlayer() const
        {
            return _p->player;
        }

        void TimelineWidget::setPlayer(const std::shared_ptr<Player>& player)
        {
            FTK_P();
            if (player == p.player)
                return;

            p.timeRange = OTIO_NS::TimeRange();
            p.playback = Playback::Stop;
            p.playbackObserver.reset();
            p.currentTimeObserver.reset();
            p.mediaReferenceKeyObserver.reset();
            p.compareObserver.reset();

            p.player = player;
            p.compare = p.player ? p.player->getCompare() : std::vector<std::shared_ptr<Timeline> >();
            p.scale = _getTimelineScale();

            _timelineUpdate();

            if (p.player)
            {
                p.timeRange = p.player->getTimeRange();

                // The compare timelines are drawn beside the player's own, so
                // the items are rebuilt when the set changes.
                p.compareObserver = ftk::ListObserver<std::shared_ptr<Timeline> >::create(
                    p.player->observeCompare(),
                    [this](const std::vector<std::shared_ptr<Timeline> >& value)
                    {
                        _p->compare = value;
                        _timelineUpdate();
                    },
                    ftk::ObserverAction::Suppress);

                p.playbackObserver = ftk::Observer<Playback>::create(
                    p.player->observePlayback(),
                    [this](Playback value)
                    {
                        _p->playback = value;
                    });

                // The clip items take their media from the timeline when they
                // are created, so they are rebuilt to follow the new media
                // reference. The items have just been built above, so the
                // current key is suppressed.
                p.mediaReferenceKeyObserver = ftk::Observer<std::string>::create(
                    p.player->observeMediaReferenceKey(),
                    [this](const std::string&)
                    {
                        _timelineUpdate();
                    },
                    ftk::ObserverAction::Suppress);

                p.currentTimeObserver = ftk::Observer<OTIO_NS::RationalTime>::create(
                    p.player->observeCurrentTime(),
                    [this](const OTIO_NS::RationalTime& value)
                    {
                        _p->currentTime = value;
                        _p->ruler->setScrollPos(
                            _p->scrollWidget->getScrollPos().x);
                        setDrawUpdate();
                        _scrollUpdate();
                    });
            }
        }

        double TimelineWidget::getViewZoom() const
        {
            return _p->scale;
        }

        void TimelineWidget::setViewZoom(double value)
        {
            const ftk::Box2I& g = getGeometry();
            setViewZoom(value, ftk::V2I(g.w() / 2, g.h() / 2));
        }

        void TimelineWidget::setViewZoom(
            double zoom,
            const ftk::V2I& focus)
        {
            FTK_P();
            _setViewZoom(
                zoom,
                p.scale,
                focus,
                p.scrollWidget->getScrollPos());
        }

        void TimelineWidget::frameView()
        {
            FTK_P();
            p.scrollWidget->setScrollPos(ftk::V2I());
            const double scale = _getTimelineScale();
            if (scale != p.scale)
            {
                p.scale = scale;
                _setItemScale();
                setSizeUpdate();
                setDrawUpdate();
            }
        }

        bool TimelineWidget::hasFrameView() const
        {
            return _p->frameView->get();
        }

        std::shared_ptr<ftk::IObservable<bool> > TimelineWidget::observeFrameView() const
        {
            return _p->frameView;
        }

        void TimelineWidget::setFrameView(bool value)
        {
            FTK_P();
            if (p.frameView->setIfChanged(value))
            {
                if (value)
                {
                    frameView();
                }
            }
        }

        bool TimelineWidget::areScrollBarsVisible() const
        {
            return _p->scrollBarsVisible->get();
        }

        std::shared_ptr<ftk::IObservable<bool> > TimelineWidget::observeScrollBarsVisible() const
        {
            return _p->scrollBarsVisible;
        }

        void TimelineWidget::setScrollBarsVisible(bool value)
        {
            FTK_P();
            if (p.scrollBarsVisible->setIfChanged(value))
            {
                _p->scrollWidget->setScrollBarsVisible(value);
            }
        }

        bool TimelineWidget::hasAutoScroll() const
        {
            return _p->autoScroll->get();
        }

        std::shared_ptr<ftk::IObservable<bool> > TimelineWidget::observeAutoScroll() const
        {
            return _p->autoScroll;
        }

        void TimelineWidget::setAutoScroll(bool value)
        {
            FTK_P();
            if (p.autoScroll->setIfChanged(value))
            {
                _scrollUpdate();
            }
        }

        void TimelineWidget::setScrollBinding(ftk::MouseButton button, ftk::KeyModifier modifier)
        {
            _p->scrollBinding = std::make_pair(button, modifier);
        }

        void TimelineWidget::setMouseWheelScale(float value)
        {
            _p->mouseWheelScale = value;
        }

        bool TimelineWidget::hasStopOnScrub() const
        {
            return _p->stopOnScrub->get();
        }

        std::shared_ptr<ftk::IObservable<bool> > TimelineWidget::observeStopOnScrub() const
        {
            return _p->stopOnScrub;
        }

        void TimelineWidget::setStopOnScrub(bool value)
        {
            FTK_P();
            if (p.stopOnScrub->setIfChanged(value))
            {
                if (!p.timelineItems.empty())
                {
                    p.timelineItems.front()->setStopOnScrub(value);
                }
            }
        }

        std::shared_ptr<ftk::IObservable<bool> > TimelineWidget::observeScrub() const
        {
            return _p->scrub;
        }

        std::shared_ptr<ftk::IObservable<std::optional<OTIO_NS::RationalTime> > > TimelineWidget::observeTimeScrub() const
        {
            return _p->timeScrub;
        }

        const std::vector<int>& TimelineWidget::getFrameMarkers() const
        {
            return _p->frameMarkers;
        }

        void TimelineWidget::setFrameMarkers(const std::vector<int>& value)
        {
            FTK_P();
            if (value == p.frameMarkers)
                return;
            p.frameMarkers = value;
            p.ruler->setFrameMarkers(value);
        }

        const ItemColors& TimelineWidget::getItemColors(int index) const
        {
            FTK_P();
            static const ItemColors empty;
            return index >= 0 && index < static_cast<int>(p.itemColors.size()) ?
                p.itemColors[index] :
                empty;
        }

        void TimelineWidget::setItemColors(int index, const ItemColors& value)
        {
            FTK_P();
            if (index < 0)
                return;
            if (index >= static_cast<int>(p.itemColors.size()))
            {
                p.itemColors.resize(index + 1);
            }
            if (value == p.itemColors[index])
                return;
            p.itemColors[index] = value;
            if (index < static_cast<int>(p.timelineItems.size()))
            {
                p.timelineItems[index]->setItemColors(value);
            }
        }

        void TimelineWidget::setLabel(int index, const std::string& value)
        {
            FTK_P();
            if (index < 0)
                return;
            if (index >= static_cast<int>(p.labels.size()))
            {
                p.labels.resize(index + 1);
            }
            if (value == p.labels[index])
                return;
            p.labels[index] = value;
            _labelsUpdate();
        }

        void TimelineWidget::_labelsUpdate()
        {
            FTK_P();

            // Nothing to tell apart when there is only one.
            const bool several = p.timelineItems.size() > 1;
            for (size_t i = 0; i < p.timelineItems.size(); ++i)
            {
                std::string label;
                if (several)
                {
                    label = i < p.labels.size() ? p.labels[i] : std::string();
                    if (label.empty())
                    {
                        const auto& timeline = 0 == i ?
                            p.player->getTimeline() :
                            p.compare[i - 1];
                        label = timeline->getPath().getFileName();
                    }
                }
                p.timelineItems[i]->setLabel(label);
            }
        }

        const ItemOptions& TimelineWidget::getItemOptions() const
        {
            return _p->itemOptions->get();
        }

        std::shared_ptr<ftk::IObservable<ItemOptions> > TimelineWidget::observeItemOptions() const
        {
            return _p->itemOptions;
        }

        void TimelineWidget::setItemOptions(const ItemOptions& value)
        {
            FTK_P();
            if (p.itemOptions->setIfChanged(value))
            {
                for (const auto& item : p.timelineItems)
                {
                    item->setOptions(value);
                }
            }
        }

        const DisplayOptions& TimelineWidget::getDisplayOptions() const
        {
            return _p->displayOptions->get();
        }

        std::shared_ptr<ftk::IObservable<DisplayOptions> > TimelineWidget::observeDisplayOptions() const
        {
            return _p->displayOptions;
        }

        void TimelineWidget::setDisplayOptions(const DisplayOptions& value)
        {
            FTK_P();
            if (p.displayOptions->setIfChanged(value))
            {
                for (const auto& item : p.timelineItems)
                {
                    item->setDisplayOptions(value);
                }
                p.ruler->setDisplayOptions(value);
                _scrollUpdate();
            }
        }
        
        ftk::Size2I TimelineWidget::getSizeHint() const
        {
            FTK_P();
            ftk::Size2I out = p.scrollWidget->getSizeHint();
            out.h += p.ruler->getSizeHint().h;
            return out;
        }

        void TimelineWidget::setGeometry(const ftk::Box2I& value)
        {
            const bool changed = value != getGeometry();
            IWidget::setGeometry(value);
            FTK_P();
            const int rulerHeight = p.ruler->getSizeHint().h;
            p.scrollWidget->setGeometry(ftk::Box2I(
                value.min.x,
                value.min.y + rulerHeight,
                value.w(),
                std::max(0, value.h() - rulerHeight)));

            // After the scroll area, whose viewport is what says how much
            // width the scroll bar left for the timelines.
            const ftk::Box2I viewport = p.scrollWidget->getScrollInfo().viewport;
            p.ruler->setGeometry(ftk::Box2I(
                viewport.min.x,
                value.min.y,
                viewport.w(),
                rulerHeight));

            if (p.sizeInit || (changed && p.frameView->get()))
            {
                p.sizeInit = false;
                frameView();
            }
            else if (!p.timelineItems.empty() &&
                p.layout->getSizeHint().w <
                p.scrollWidget->getScrollInfo().viewport.w())
            {
                setFrameView(true);
                frameView();
            }
        }

        void TimelineWidget::sizeHintEvent(const ftk::SizeHintEvent& event)
        {
            FTK_P();
            p.sizeInit |= event.displayScale != p.displayScale;
            p.displayScale = event.displayScale;
            p.border = event.style->getSizeRole(
                ftk::SizeRole::Border, event.displayScale);
        }

        void TimelineWidget::drawOverlayEvent(
            const ftk::Box2I& drawRect,
            const ftk::DrawEvent& event)
        {
            IWidget::drawOverlayEvent(drawRect, event);
            FTK_P();
            if (!p.player)
                return;

            // Over every timeline rather than only the one being played: the
            // same instant marked down the stack is what says where the
            // versions are being compared.
            const ftk::Box2I& g = p.scrollWidget->getGeometry();
            const int x = p.ruler->timeToPos(p.currentTime);
            if (x >= g.min.x && x <= g.max.x)
            {
                event.render->drawRect(
                    ftk::Box2I(x, g.min.y, p.border * 2, g.h()),
                    event.style->getColorRole(ftk::ColorRole::Red));
            }
        }

        void TimelineWidget::mouseEnterEvent(ftk::MouseEnterEvent& event)
        {
            FTK_P();
            event.accept = true;
            p.mouse.inside = true;
        }

        void TimelineWidget::mouseLeaveEvent()
        {
            FTK_P();
            p.mouse.inside = false;
        }

        void TimelineWidget::mouseMoveEvent(ftk::MouseMoveEvent& event)
        {
            FTK_P();
            switch (p.mouse.mode)
            {
            case Private::MouseMode::Scroll:
            {
                event.accept = true;
                const ftk::V2I d = event.pos - p.mouse.press;
                p.scrollWidget->setScrollPos(p.mouse.scrollPos - d);
                setFrameView(false);
                break;
            }
            default: break;
            }
        }

        void TimelineWidget::mousePressEvent(ftk::MouseClickEvent& event)
        {
            FTK_P();
            p.mouse.press = event.pos;
            if (p.itemOptions->get().inputEnabled &&
                p.scrollBinding.first == event.button &&
                checkKeyModifier(p.scrollBinding.second, event.modifiers))
            {
                // Only claim the button when it is bound to scrolling.
                // Accepting a button we do nothing with would deny it to
                // everything else, including a context menu.
                event.accept = true;
                p.mouse.mode = Private::MouseMode::Scroll;
                p.mouse.scrollPos = p.scrollWidget->getScrollPos();
            }
            else
            {
                p.mouse.mode = Private::MouseMode::None;
            }
        }

        void TimelineWidget::mouseReleaseEvent(ftk::MouseClickEvent& event)
        {
            FTK_P();
            event.accept = true;
            p.mouse.mode = Private::MouseMode::None;
        }

        void TimelineWidget::scrollEvent(ftk::ScrollEvent& event)
        {
            FTK_P();
            if (p.itemOptions->get().inputEnabled)
            {
                event.accept = true;
                const double newZoom =
                    event.value.y > 0 ?
                    p.scale * p.mouseWheelScale :
                    p.scale / p.mouseWheelScale;
                setViewZoom(newZoom, event.pos);
            }
        }

        void TimelineWidget::keyPressEvent(ftk::KeyEvent& event)
        {
            FTK_P();
            if (p.itemOptions->get().inputEnabled &&
                0 == event.modifiers)
            {
                switch (event.key)
                {
                case ftk::Key::Equals:
                    event.accept = true;
                    setViewZoom(p.scale * 2.F, event.pos);
                    break;
                case ftk::Key::Minus:
                    event.accept = true;
                    setViewZoom(p.scale / 2.F, event.pos);
                    break;
                case ftk::Key::Backspace:
                    event.accept = true;
                    setFrameView(true);
                    break;
                default: break;
                }
            }
        }

        void TimelineWidget::keyReleaseEvent(ftk::KeyEvent& event)
        {
            event.accept = true;
        }

        void TimelineWidget::_setViewZoom(
            double zoomNew,
            double zoomPrev,
            const ftk::V2I& focus,
            const ftk::V2I& scrollPos)
        {
            FTK_P();
            const double zoomMin = _getTimelineScale();
            const double zoomMax = _getTimelineScaleMax();
            const double zoomClamped = ftk::clamp(zoomNew, zoomMin, zoomMax);
            if (zoomClamped != p.scale)
            {
                p.scale = zoomClamped;
                _setItemScale();
                const double s = zoomClamped / zoomPrev;
                const ftk::V2I scrollPosNew(
                    (scrollPos.x + focus.x) * s - focus.x,
                    scrollPos.y);
                p.scrollWidget->setScrollPos(scrollPosNew, false);

                setFrameView(zoomNew <= zoomMin);
            }
        }

        double TimelineWidget::_getDuration() const
        {
            FTK_P();

            // The longest of them, so that framing the view frames all of
            // them: a shorter timeline stops early and leaves the rest of its
            // row empty, which is the difference being looked for.
            double out = 0.0;
            if (p.player)
            {
                out = p.player->getTimeRange().duration().rescaled_to(1.0).value();
            }
            for (const auto& timeline : p.compare)
            {
                out = std::max(
                    out,
                    timeline->getTimeRange().duration().rescaled_to(1.0).value());
            }
            return out;
        }

        double TimelineWidget::_getTimelineScale() const
        {
            FTK_P();
            double out = 1.0;
            if (p.player)
            {
                const double duration = _getDuration();
                if (duration > 0.0)
                {
                    const ftk::Box2I scrollViewport = p.scrollWidget->getScrollInfo().viewport;
                    out = scrollViewport.w() / duration;
                }
            }
            return out;
        }

        double TimelineWidget::_getTimelineScaleMax() const
        {
            FTK_P();
            double out = 1.0;
            if (p.player)
            {
                const ftk::Box2I scrollViewport = p.scrollWidget->getScrollInfo().viewport;
                const double duration = _getDuration();
                if (duration < 1.0)
                {
                    if (duration > 0.0)
                    {
                        out = scrollViewport.w() / duration;
                    }
                }
                else
                {
                    out = scrollViewport.w();
                }
            }
            return out;
        }

        void TimelineWidget::_setItemScale()
        {
            FTK_P();
            for (const auto& item : p.timelineItems)
            {
                item->setScale(p.scale);
            }
            p.ruler->setScale(p.scale);
            p.ruler->setScrollPos(p.scrollWidget->getScrollPos().x);
        }

        void TimelineWidget::_scrollUpdate()
        {
            FTK_P();
            // Follows the player's current time; the timelines it is compared
            // against have no playhead of their own to follow.
            if (!p.timelineItems.empty() &&
                p.autoScroll->get() &&
                !p.scrub->get() &&
                Private::MouseMode::None == p.mouse.mode)
            {
                ftk::V2I scrollPos = p.scrollWidget->getScrollPos();
                const OTIO_NS::RationalTime t = p.currentTime - p.timeRange.start_time();
                const int pos =
                    getGeometry().min.x -
                    scrollPos.x +
                    t.rescaled_to(1.0).value() * p.scale;

                const ftk::Box2I vp = p.scrollWidget->getScrollInfo().viewport;
                const int margin = vp.w() * marginPercentage;
                if (pos < (vp.min.x + margin) || pos >(vp.max.x - margin))
                {
                    const int offset = pos < (vp.min.x + margin) ? (vp.min.x + margin) : (vp.max.x - margin);
                    scrollPos.x = getGeometry().min.x - offset + t.rescaled_to(1.0).value() * p.scale;
                    p.scrollWidget->setScrollPos(scrollPos);
                }
            }
            p.scrollWidget->setScrollType(p.displayOptions->get().minimize ?
                ftk::ScrollType::Horizontal :
                ftk::ScrollType::Both);
        }

        std::shared_ptr<ItemData> TimelineWidget::_getItemData(
            const std::shared_ptr<Timeline>& timeline) const
        {
            FTK_P();
            auto out = std::make_shared<ItemData>();
            out->timeUnitsModel = p.timeUnitsModel;
            out->speed = timeline->getTimeRange().duration().rate();
            out->dir = timeline->getPath().getDir();
            out->options = timeline->getOptions();

            // Weak, because the item data belongs to this widget and the
            // timeline does not.
            std::weak_ptr<Timeline> weak = timeline;
            out->toMediaTime =
                [weak](const OTIO_NS::RationalTime& value)
                {
                    if (auto timeline = weak.lock())
                    {
                        if (const auto time = timeline->getMediaTime(value))
                        {
                            return *time;
                        }
                    }
                    return value;
                };
            return out;
        }

        void TimelineWidget::_timelineUpdate()
        {
            FTK_P();

            const ftk::V2I scrollPos = p.scrollWidget->getScrollPos();

            p.scrubObserver.reset();
            p.timeScrubObserver.reset();
            for (const auto& item : p.timelineItems)
            {
                item->setParent(nullptr);
            }
            p.timelineItems.clear();
            p.itemData.clear();
            p.ruler->setPlayer(nullptr);

            if (p.player)
            {
                if (auto context = getContext())
                {
                    // The player's own timeline first, then what it is being
                    // compared against, so that the item index and the compare
                    // index agree with each other.
                    p.itemData.push_back(_getItemData(p.player->getTimeline()));
                    p.timelineItems.push_back(TimelineItem::create(
                        context,
                        p.player,
                        p.scale,
                        p.itemOptions->get(),
                        p.displayOptions->get(),
                        p.itemData.back(),
                        p.layout));
                    for (const auto& timeline : p.compare)
                    {
                        p.itemData.push_back(_getItemData(timeline));
                        p.timelineItems.push_back(TimelineItem::create(
                            context,
                            timeline,
                            p.scale,
                            p.itemOptions->get(),
                            p.displayOptions->get(),
                            p.itemData.back(),
                            p.layout));
                    }

                    p.timelineItems.front()->setStopOnScrub(p.stopOnScrub->get());
                    p.timelineItems.front()->setFrameMarkers(p.frameMarkers);
                    for (size_t i = 0; i < p.timelineItems.size(); ++i)
                    {
                        if (i < p.itemColors.size())
                        {
                            p.timelineItems[i]->setItemColors(p.itemColors[i]);
                        }
                    }
                    _labelsUpdate();

                    // The ruler is the player's, so it names its times the
                    // way the player's own timeline does.
                    p.ruler->setItemData(p.itemData.front());
                    p.ruler->setPlayer(p.player);
                    p.ruler->setScale(p.scale);
                    p.ruler->setFrameMarkers(p.frameMarkers);
                    p.ruler->setDisplayOptions(p.displayOptions->get());

                    p.scrollWidget->setScrollPos(scrollPos);

                    // Only the player's item can be scrubbed, so it is the
                    // only one there is anything to hear from.
                    p.scrubObserver = ftk::Observer<bool>::create(
                        p.timelineItems.front()->observeScrub(),
                        [this](bool value)
                        {
                            _p->scrub->setIfChanged(value);
                            _scrollUpdate();
                        });

                    p.timeScrubObserver = ftk::Observer<std::optional<OTIO_NS::RationalTime> >::create(
                        p.timelineItems.front()->observeTimeScrub(),
                        [this](const std::optional<OTIO_NS::RationalTime>& value)
                        {
                            _p->timeScrub->setIfChanged(value);
                        });
                }
            }
        }
    }
}

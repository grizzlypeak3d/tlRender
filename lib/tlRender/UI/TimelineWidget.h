// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/UI/TimelineItem.h>

namespace tl
{
    namespace ui
    {
        //! Timeline widget.
        class TL_API_TYPE TimelineWidget : public ftk::IWidget
        {
            FTK_NON_COPYABLE(TimelineWidget);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<ITimeUnitsModel>&,
                const std::shared_ptr<IWidget>& parent);

            TimelineWidget();

        public:
            TL_API virtual ~TimelineWidget();

            //! Create a new widget.
            TL_API static std::shared_ptr<TimelineWidget> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

            //! Create a new widget.
            TL_API static std::shared_ptr<TimelineWidget> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<ITimeUnitsModel>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

            //! Get the time units model.
            TL_API const std::shared_ptr<ITimeUnitsModel>& getTimeUnitsModel() const;

            //! Get the timeline player.
            TL_API std::shared_ptr<Player>& getPlayer() const;

            //! Set the timeline player.
            TL_API void setPlayer(const std::shared_ptr<Player>&);

            //! \name View
            ///@{

            //! Set the view zoom.
            TL_API void setViewZoom(double);

            //! Set the view zoom.
            TL_API void setViewZoom(
                double,
                const ftk::V2I& focus);

            //! Frame the view.
            TL_API void frameView();

            //! Get whether the view is framed automatically.
            TL_API bool hasFrameView() const;
            
            //! Observe whether the view is framed automatically.
            TL_API std::shared_ptr<ftk::IObservable<bool> > observeFrameView() const;

            //! Set whether the view is framed automatically.
            TL_API void setFrameView(bool);

            //! Get whether the scroll bars are visible.
            TL_API bool areScrollBarsVisible() const;

            //! Observe whether the scroll bars are visible.
            TL_API std::shared_ptr<ftk::IObservable<bool> > observeScrollBarsVisible() const;

            //! Set whether the scroll bars are visible.
            TL_API void setScrollBarsVisible(bool);

            //! Get whether auto-scroll is enabled.
            TL_API bool hasAutoScroll() const;

            //! Observe whether auto-scroll is enabled.
            TL_API std::shared_ptr<ftk::IObservable<bool> > observeAutoScroll() const;

            //! Set whether auto-scroll is enabled.
            TL_API void setAutoScroll(bool);

            //! Set the scroll binding.
            TL_API void setScrollBinding(ftk::MouseButton, ftk::KeyModifier);

            //! Set the mouse wheel scale.
            TL_API void setMouseWheelScale(float);

            ///@}

            //! \name Scrubbing
            ///@{

            //! Get whether to stop playback when scrubbing.
            TL_API bool hasStopOnScrub() const;

            //! Observe whether to stop playback when scrubbing.
            TL_API std::shared_ptr<ftk::IObservable<bool> > observeStopOnScrub() const;

            //! Set whether to stop playback when scrubbing.
            TL_API void setStopOnScrub(bool);

            //! Observe whether scrubbing is in progress.
            TL_API std::shared_ptr<ftk::IObservable<bool> > observeScrub() const;

            //! Observe time scrubbing.
            TL_API std::shared_ptr<ftk::IObservable<std::optional<OTIO_NS::RationalTime> > > observeTimeScrub() const;

            ///@}

            //! \name Frame Markers
            ///@{

            //! Get the frame markers.
            TL_API const std::vector<int>& getFrameMarkers() const;

            //! Set the frame markers.
            TL_API void setFrameMarkers(const std::vector<int>&);

            ///@}

            //! \name Options
            ///@{

            //! Get the item options.
            TL_API const ItemOptions& getItemOptions() const;

            //! Observe the item options.
            TL_API std::shared_ptr<ftk::IObservable<ItemOptions> > observeItemOptions() const;

            //! Set the item options.
            TL_API void setItemOptions(const ItemOptions&);

            //! Get the display options.
            TL_API const DisplayOptions& getDisplayOptions() const;

            //! Observe the display options.
            TL_API std::shared_ptr<ftk::IObservable<DisplayOptions> > observeDisplayOptions() const;

            //! Set the display options.
            TL_API void setDisplayOptions(const DisplayOptions&);

            ///@}

            TL_API ftk::Size2I getSizeHint() const override;
            TL_API void setGeometry(const ftk::Box2I&) override;
            TL_API void sizeHintEvent(const ftk::SizeHintEvent&) override;
            TL_API void mouseEnterEvent(ftk::MouseEnterEvent&) override;
            TL_API void mouseLeaveEvent() override;
            TL_API void mouseMoveEvent(ftk::MouseMoveEvent&) override;
            TL_API void mousePressEvent(ftk::MouseClickEvent&) override;
            TL_API void mouseReleaseEvent(ftk::MouseClickEvent&) override;
            TL_API void scrollEvent(ftk::ScrollEvent&) override;
            TL_API void keyPressEvent(ftk::KeyEvent&) override;
            TL_API void keyReleaseEvent(ftk::KeyEvent&) override;

        private:
            void _setViewZoom(
                double zoomNew,
                double zoomPrev,
                const ftk::V2I& focus,
                const ftk::V2I& scrollPos);

            double _getTimelineScale() const;
            double _getTimelineScaleMax() const;

            void _setItemScale();

            void _scrollUpdate();
            void _timelineUpdate();

            FTK_PRIVATE();
        };
    }
}

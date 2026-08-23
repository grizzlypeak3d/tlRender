// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/UI/Export.h>
#include <tlRender/UI/TimelineItem.h>

namespace tl
{
    namespace ui
    {
        //! Timeline widget.
        class TL_UI_API_TYPE TimelineWidget : public ftk::IWidget
        {
            FTK_NON_COPYABLE(TimelineWidget);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<ITimeUnitsModel>&,
                const std::shared_ptr<IWidget>& parent);

            TimelineWidget();

        public:
            TL_UI_API virtual ~TimelineWidget();

            //! Create a new widget.
            TL_UI_API static std::shared_ptr<TimelineWidget> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

            //! Create a new widget.
            TL_UI_API static std::shared_ptr<TimelineWidget> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<ITimeUnitsModel>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

            //! Get the time units model.
            TL_UI_API const std::shared_ptr<ITimeUnitsModel>& getTimeUnitsModel() const;

            //! Get the timeline player.
            TL_UI_API std::shared_ptr<Player>& getPlayer() const;

            //! Set the timeline player.
            TL_UI_API void setPlayer(const std::shared_ptr<Player>&);

            //! Set the timelines drawn beside the player's own.
            //!
            //! Unset, they are the player's comparison timelines, which is
            //! what a player comparing pictures is showing. Set, they are
            //! whatever is given: for drawing the structure of timelines the
            //! picture is not comparing, which the player has no reason to
            //! know about.
            TL_UI_API void setTimelines(
                const std::optional<std::vector<std::shared_ptr<Timeline> > >&);

            //! \name View
            ///@{

            //! Get the view zoom, in pixels per second.
            TL_UI_API double getViewZoom() const;

            //! Set the view zoom.
            TL_UI_API void setViewZoom(double);

            //! Set the view zoom.
            TL_UI_API void setViewZoom(
                double,
                const ftk::V2I& focus);

            //! Frame the view.
            TL_UI_API void frameView();

            //! Get whether the view is framed automatically.
            TL_UI_API bool hasFrameView() const;
            
            //! Observe whether the view is framed automatically.
            TL_UI_API std::shared_ptr<ftk::IObservable<bool> > observeFrameView() const;

            //! Set whether the view is framed automatically.
            TL_UI_API void setFrameView(bool);

            //! Get whether the scroll bars are visible.
            TL_UI_API bool areScrollBarsVisible() const;

            //! Observe whether the scroll bars are visible.
            TL_UI_API std::shared_ptr<ftk::IObservable<bool> > observeScrollBarsVisible() const;

            //! Set whether the scroll bars are visible.
            TL_UI_API void setScrollBarsVisible(bool);

            //! Get whether auto-scroll is enabled.
            TL_UI_API bool hasAutoScroll() const;

            //! Observe whether auto-scroll is enabled.
            TL_UI_API std::shared_ptr<ftk::IObservable<bool> > observeAutoScroll() const;

            //! Set whether auto-scroll is enabled.
            TL_UI_API void setAutoScroll(bool);

            //! Set the scroll binding.
            TL_UI_API void setScrollBinding(ftk::MouseButton, ftk::KeyModifier);

            //! Set the mouse wheel scale.
            TL_UI_API void setMouseWheelScale(float);

            ///@}

            //! \name Scrubbing
            ///@{

            //! Get whether to stop playback when scrubbing.
            TL_UI_API bool hasStopOnScrub() const;

            //! Observe whether to stop playback when scrubbing.
            TL_UI_API std::shared_ptr<ftk::IObservable<bool> > observeStopOnScrub() const;

            //! Set whether to stop playback when scrubbing.
            TL_UI_API void setStopOnScrub(bool);

            //! Observe whether scrubbing is in progress.
            TL_UI_API std::shared_ptr<ftk::IObservable<bool> > observeScrub() const;

            //! Observe time scrubbing.
            TL_UI_API std::shared_ptr<ftk::IObservable<std::optional<OTIO_NS::RationalTime> > > observeTimeScrub() const;

            ///@}

            //! \name Frame Markers
            ///@{

            //! Get the frame markers.
            TL_UI_API const std::vector<int>& getFrameMarkers() const;

            //! Set the frame markers.
            TL_UI_API void setFrameMarkers(const std::vector<int>&);

            ///@}

            //! \name Item Colors
            ///@{

            //! Get the item colors for the given timeline, zero being the
            //! player's own and the rest what it is compared against.
            TL_UI_API const ItemColors& getItemColors(int index) const;

            //! Set colors marking items.
            //!
            //! For an embedder that knows something about the items the
            //! timeline does not -- which of them a comparison found changed,
            //! say. Kept outside the timeline because the players read it on
            //! their own threads and it is not ours to write to.
            //!
            //! Drawn as an outline around the item, so that the color the
            //! timeline was authored with is still the color of the item.
            TL_UI_API void setItemColors(int index, const ItemColors&);

            ///@}

            //! \name Alignment
            ///@{

            //! Set how far along the given timeline is drawn, zero being the
            //! player's own and the rest what it is compared against.
            //!
            //! For lining the timelines up by what is in them rather than by
            //! when it happens. The ruler follows the player's own, so the
            //! times over a timeline still name the times in it.
            TL_UI_API void setOffset(int index, const OTIO_NS::RationalTime&);

            ///@}

            //! \name Labels
            ///@{

            //! Set the label naming the given timeline, zero being the
            //! player's own and the rest what it is compared against.
            //!
            //! Only drawn when more than one timeline is shown, since one on
            //! its own does not need telling apart. Left unset a timeline is
            //! named after its file, which is what the label is for; set it
            //! to say more than the file name does.
            TL_UI_API void setLabel(int index, const std::string&);

            ///@}

            //! \name Options
            ///@{

            //! Get the item options.
            TL_UI_API const ItemOptions& getItemOptions() const;

            //! Observe the item options.
            TL_UI_API std::shared_ptr<ftk::IObservable<ItemOptions> > observeItemOptions() const;

            //! Set the item options.
            TL_UI_API void setItemOptions(const ItemOptions&);

            //! Get the display options.
            TL_UI_API const DisplayOptions& getDisplayOptions() const;

            //! Observe the display options.
            TL_UI_API std::shared_ptr<ftk::IObservable<DisplayOptions> > observeDisplayOptions() const;

            //! Set the display options.
            TL_UI_API void setDisplayOptions(const DisplayOptions&);

            ///@}

            TL_UI_API ftk::Size2I getSizeHint() const override;
            TL_UI_API void setGeometry(const ftk::Box2I&) override;
            TL_UI_API void sizeHintEvent(const ftk::SizeHintEvent&) override;
            TL_UI_API void drawOverlayEvent(const ftk::Box2I&, const ftk::DrawEvent&) override;
            TL_UI_API void mouseEnterEvent(ftk::MouseEnterEvent&) override;
            TL_UI_API void mouseLeaveEvent() override;
            TL_UI_API void mouseMoveEvent(ftk::MouseMoveEvent&) override;
            TL_UI_API void mousePressEvent(ftk::MouseClickEvent&) override;
            TL_UI_API void mouseReleaseEvent(ftk::MouseClickEvent&) override;
            TL_UI_API void scrollEvent(ftk::ScrollEvent&) override;
            TL_UI_API void keyPressEvent(ftk::KeyEvent&) override;
            TL_UI_API void keyReleaseEvent(ftk::KeyEvent&) override;

        private:
            void _setViewZoom(
                double zoomNew,
                double zoomPrev,
                const ftk::V2I& focus,
                const ftk::V2I& scrollPos);

            std::vector<std::shared_ptr<Timeline> > _getCompare() const;

            std::shared_ptr<ItemData> _getItemData(
                const std::shared_ptr<Timeline>&) const;

            double _getDuration() const;
            double _getTimelineScale() const;
            double _getTimelineScaleMax() const;

            void _setItemScale();
            void _labelsUpdate();
            void _offsetsUpdate();

            void _scrollUpdate();
            void _timelineUpdate();

            FTK_PRIVATE();
        };
    }
}

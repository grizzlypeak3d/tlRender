// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/UI/Export.h>
#include <tlRender/UI/ItemOptions.h>

#include <tlRender/Timeline/Player.h>

#include <ftk/UI/IMouseWidget.h>

namespace tl
{
    namespace ui
    {
        //! Track types.
        enum class TL_UI_API_TYPE TrackType
        {
            None,
            Video,
            Audio
        };

        //! Timeline item.
        //!
        //! This draws a timeline's tracks: every clip and gap, with the
        //! thumbnails and waveforms in them. The clips and gaps are data
        //! rather than widgets of their own, which keeps the cost of a frame
        //! proportional to what is on screen instead of to the number of
        //! items in the timeline.
        //!
        //! The time ruler is not here but above the items, in TimelineRuler:
        //! timelines shown together share one.
        class TL_UI_API_TYPE TimelineItem : public ftk::IMouseWidget
        {
        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<Timeline>&,
                const std::shared_ptr<Player>&,
                double scale,
                const ItemOptions&,
                const DisplayOptions&,
                const std::shared_ptr<ItemData>&,
                const std::shared_ptr<IWidget>& parent);

            TimelineItem();

        public:
            TL_UI_API virtual ~TimelineItem();

            //! Create a new item for a timeline that is being played.
            TL_UI_API static std::shared_ptr<TimelineItem> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<Player>&,
                double scale,
                const ItemOptions&,
                const DisplayOptions&,
                const std::shared_ptr<ItemData>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

            //! Create a new item for a timeline that is not being played.
            //!
            //! For showing a timeline alongside the one being played -- an
            //! earlier version of it, say. It cannot be scrubbed: there is
            //! nothing to seek.
            TL_UI_API static std::shared_ptr<TimelineItem> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<Timeline>&,
                double scale,
                const ItemOptions&,
                const DisplayOptions&,
                const std::shared_ptr<ItemData>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

            //! Set whether playback stops when scrubbing.
            TL_UI_API void setStopOnScrub(bool);

            //! Observe whether scrubbing is in progress.
            TL_UI_API std::shared_ptr<ftk::IObservable<bool> > observeScrub() const;

            //! Observe time scrubbing.
            TL_UI_API std::shared_ptr<ftk::IObservable<std::optional<OTIO_NS::RationalTime> > > observeTimeScrub() const;

            //! Set the frame markers.
            TL_UI_API void setFrameMarkers(const std::vector<int>&);

            //! Set colors marking items.
            //!
            //! Drawn as an outline around the item rather than as its color:
            //! the item's own color says what kind of item it is, and may be
            //! one the timeline was authored with.
            TL_UI_API void setItemColors(const ItemColors&);

            //! Set the label naming this timeline.
            //!
            //! Drawn above the tracks when it is not empty, and taking up no
            //! room when it is. A timeline shown on its own does not need
            //! naming; several drawn together do.
            TL_UI_API void setLabel(const std::string&);

            //! Get the time range.
            TL_UI_API const OTIO_NS::TimeRange& getTimeRange() const;

            //! Set the scale, in pixels per second.
            TL_UI_API void setScale(double);

            //! Set how far along the timeline is drawn.
            //!
            //! For lining several timelines up by what is in them rather than
            //! by when it happens: a version with material added near the
            //! head holds the same clips as the one before it, later, and an
            //! offset puts them back under each other. Must not be negative;
            //! there is nothing to the left of the start to draw into.
            TL_UI_API void setOffset(const OTIO_NS::RationalTime&);

            //! Set the options.
            TL_UI_API void setOptions(const ItemOptions&);

            //! Set the display options.
            TL_UI_API void setDisplayOptions(const DisplayOptions&);

            //! Convert a position to a time.
            TL_UI_API OTIO_NS::RationalTime posToTime(float) const;

            //! Convert a time to a position.
            TL_UI_API int timeToPos(const OTIO_NS::RationalTime&) const;

            TL_UI_API ftk::Size2I getSizeHint() const override;
            TL_UI_API void setGeometry(const ftk::Box2I&) override;
            TL_UI_API void styleEvent(const ftk::StyleEvent&) override;
            TL_UI_API void sizeHintEvent(const ftk::SizeHintEvent&) override;
            TL_UI_API void clipEvent(const ftk::Box2I&, bool) override;
            TL_UI_API void tickEvent(
                bool,
                bool,
                const ftk::TickEvent&) override;
            TL_UI_API void drawEvent(const ftk::Box2I&, const ftk::DrawEvent&) override;
            TL_UI_API void drawOverlayEvent(const ftk::Box2I&, const ftk::DrawEvent&) override;
            TL_UI_API void mouseMoveEvent(ftk::MouseMoveEvent&) override;
            TL_UI_API void mousePressEvent(ftk::MouseClickEvent&) override;
            TL_UI_API void mouseReleaseEvent(ftk::MouseClickEvent&) override;

        private:
            void _timeUnitsUpdate();

            bool _isTrackVisible(int) const;

            //! Scale a rectangle about its center, used to ask for a wider band
            //! of thumbnails than is drawn.
            static ftk::Box2I _getClipRect(
                const ftk::Box2I&,
                double scale);

            std::string _getDurationLabel(const OTIO_NS::RationalTime&) const;

            //! \name Items
            ///@{

            void _itemsInit(const std::shared_ptr<ftk::Context>&);
            void _itemsScaleUpdate();
            void _itemsTextUpdate(const ftk::SizeHintEvent&);
            void _visibleUpdate(const ftk::Box2I& drawRect);
            void _requestsUpdate();
            void _cancelRequests();

            void _drawItems(
                const ftk::Box2I&,
                const ftk::DrawEvent&);
            void _drawThumbnails(
                const ftk::Box2I&,
                const ftk::DrawEvent&);
            void _drawItemLabels(
                const ftk::Box2I&,
                const ftk::DrawEvent&);

            ///@}

            void _tracksUpdate();
            void _textUpdate();

            OTIO_NS::TimeRange _timeRange;
            OTIO_NS::RationalTime _offset;
            double _scale = 500.0;
            ItemOptions _options;
            DisplayOptions _displayOptions;
            std::shared_ptr<ItemData> _data;

            FTK_PRIVATE();
        };
    }
}

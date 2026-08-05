// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/UI/ItemOptions.h>

#include <tlRender/Timeline/Player.h>

#include <ftk/UI/IMouseWidget.h>

namespace tl
{
    namespace ui
    {
        //! Track types.
        enum class TL_API_TYPE TrackType
        {
            None,
            Video,
            Audio
        };

        //! Timeline item.
        //!
        //! This draws the whole timeline: the time ruler, the cache and in/out
        //! bars, and every clip and gap. The clips and gaps are data rather
        //! than widgets of their own, which keeps the cost of a frame
        //! proportional to what is on screen instead of to the number of items
        //! in the timeline.
        class TL_API_TYPE TimelineItem : public ftk::IMouseWidget
        {
        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<Player>&,
                double scale,
                const ItemOptions&,
                const DisplayOptions&,
                const std::shared_ptr<ItemData>&,
                const std::shared_ptr<IWidget>& parent);

            TimelineItem();

        public:
            TL_API virtual ~TimelineItem();

            //! Create a new item.
            TL_API static std::shared_ptr<TimelineItem> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<Player>&,
                double scale,
                const ItemOptions&,
                const DisplayOptions&,
                const std::shared_ptr<ItemData>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

            //! Set whether playback stops when scrubbing.
            TL_API void setStopOnScrub(bool);

            //! Observe whether scrubbing is in progress.
            TL_API std::shared_ptr<ftk::IObservable<bool> > observeScrub() const;

            //! Observe time scrubbing.
            TL_API std::shared_ptr<ftk::IObservable<std::optional<OTIO_NS::RationalTime> > > observeTimeScrub() const;

            //! Set the frame markers.
            TL_API void setFrameMarkers(const std::vector<int>&);

            //! Set colors for items.
            TL_API void setItemColors(const ItemColors&);

            //! Get the time range.
            TL_API const OTIO_NS::TimeRange& getTimeRange() const;

            //! Set the scale, in pixels per second.
            TL_API void setScale(double);

            //! Set the options.
            TL_API void setOptions(const ItemOptions&);

            //! Set the display options.
            TL_API void setDisplayOptions(const DisplayOptions&);

            //! Convert a position to a time.
            TL_API OTIO_NS::RationalTime posToTime(float) const;

            //! Convert a time to a position.
            TL_API int timeToPos(const OTIO_NS::RationalTime&) const;

            TL_API ftk::Size2I getSizeHint() const override;
            TL_API void setGeometry(const ftk::Box2I&) override;
            TL_API void styleEvent(const ftk::StyleEvent&) override;
            TL_API void sizeHintEvent(const ftk::SizeHintEvent&) override;
            TL_API void clipEvent(const ftk::Box2I&, bool) override;
            TL_API void tickEvent(
                bool,
                bool,
                const ftk::TickEvent&) override;
            TL_API void drawEvent(const ftk::Box2I&, const ftk::DrawEvent&) override;
            TL_API void drawOverlayEvent(const ftk::Box2I&, const ftk::DrawEvent&) override;
            TL_API void mouseMoveEvent(ftk::MouseMoveEvent&) override;
            TL_API void mousePressEvent(ftk::MouseClickEvent&) override;
            TL_API void mouseReleaseEvent(ftk::MouseClickEvent&) override;

        private:
            void _timeUnitsUpdate();

            bool _isTrackVisible(int) const;

            //! Scale a rectangle about its center, used to ask for a wider band
            //! of thumbnails than is drawn.
            static ftk::Box2I _getClipRect(
                const ftk::Box2I&,
                double scale);

            std::string _timeLabel(const OTIO_NS::RationalTime&) const;
            std::string _getDurationLabel(const OTIO_NS::RationalTime&) const;

            ftk::Size2I _getLabelMaxSize(const std::shared_ptr<ftk::FontSystem>&) const;
            double _getSecondsInc(const std::shared_ptr<ftk::FontSystem>&);

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

            void _drawInOutPoints(
                const ftk::Box2I&,
                const ftk::DrawEvent&);
            void _drawFrameMarkers(
                const ftk::Box2I&,
                const ftk::DrawEvent&);
            void _drawCacheInfo(
                const ftk::Box2I&,
                const ftk::DrawEvent&);
            void _drawTimeLabels(
                const ftk::Box2I&,
                const ftk::DrawEvent&);
            void _drawTimeTicks(
                const ftk::Box2I&,
                const ftk::DrawEvent&);
            void _drawCurrentTime(
                const ftk::Box2I&,
                const ftk::DrawEvent&);

            void _tracksUpdate();
            void _textUpdate();

            OTIO_NS::TimeRange _timeRange;
            double _scale = 500.0;
            ItemOptions _options;
            DisplayOptions _displayOptions;
            std::shared_ptr<ItemData> _data;

            FTK_PRIVATE();
        };
    }
}

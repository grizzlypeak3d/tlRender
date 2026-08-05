// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/UI/ItemOptions.h>

#include <tlRender/Timeline/Player.h>

#include <ftk/UI/IWidget.h>

namespace tl
{
    namespace ui
    {
        //! The time ruler drawn above the timelines.
        //!
        //! Outside the scrolled timelines rather than part of one: several
        //! timelines shown together share a time axis, and one drawn inside
        //! the scroll area either scrolls away with its timeline or has to be
        //! pinned back to the top, where the others land on it.
        //!
        //! It is the player's ruler. The timelines the player is compared
        //! against are read against its axis, and the in/out points, frame
        //! markers and cache drawn here are its own.
        class TL_API_TYPE TimelineRuler : public ftk::IWidget
        {
            FTK_NON_COPYABLE(TimelineRuler);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<ItemData>&,
                const std::shared_ptr<IWidget>& parent);

            TimelineRuler();

        public:
            TL_API virtual ~TimelineRuler();

            //! Create a new widget.
            TL_API static std::shared_ptr<TimelineRuler> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<ItemData>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

            //! Set the item data, which the labels are named through.
            TL_API void setItemData(const std::shared_ptr<ItemData>&);

            //! Set the player.
            TL_API void setPlayer(const std::shared_ptr<Player>&);

            //! Set the scale, in pixels per second.
            TL_API void setScale(double);

            //! Set how far the timelines are scrolled, so that the ruler is
            //! over the part of them it names.
            TL_API void setScrollPos(int);

            //! Set the frame markers.
            TL_API void setFrameMarkers(const std::vector<int>&);

            //! Set the display options.
            TL_API void setDisplayOptions(const DisplayOptions&);

            //! Convert a time to a position.
            TL_API int timeToPos(const OTIO_NS::RationalTime&) const;

            TL_API ftk::Size2I getSizeHint() const override;
            TL_API void sizeHintEvent(const ftk::SizeHintEvent&) override;
            TL_API void drawEvent(const ftk::Box2I&, const ftk::DrawEvent&) override;

        private:
            void _drawInOutPoints(const ftk::Box2I&, const ftk::DrawEvent&);
            void _drawFrameMarkers(const ftk::Box2I&, const ftk::DrawEvent&);
            void _drawCacheInfo(const ftk::Box2I&, const ftk::DrawEvent&);
            void _drawTimeLabels(const ftk::Box2I&, const ftk::DrawEvent&);
            void _drawTimeTicks(const ftk::Box2I&, const ftk::DrawEvent&);
            void _drawCurrentTime(const ftk::Box2I&, const ftk::DrawEvent&);

            OTIO_NS::RationalTime _posToTime(float) const;
            std::string _timeLabel(const OTIO_NS::RationalTime&) const;
            ftk::Size2I _getLabelMaxSize(const std::shared_ptr<ftk::FontSystem>&) const;
            double _getSecondsInc(const std::shared_ptr<ftk::FontSystem>&);

            FTK_PRIVATE();
        };
    }
}

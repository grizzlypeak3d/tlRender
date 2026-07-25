// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UI/IBasicItem.h>

#include <ftk/UI/DrawUtil.h>
#include <ftk/Core/RenderUtil.h>

namespace tl
{
    namespace ui
    {
        struct IBasicItem::Private
        {
            std::string label;
            std::string durationLabel;
            ftk::Color4F color;
            std::vector<Marker> markers;

            struct SizeData
            {
                bool init = true;
                int margin = 0;
                int border = 0;
                ftk::FontInfo fontInfo;
                ftk::FontMetrics fontMetrics;
                ftk::Size2I labelSize;
                ftk::Size2I durationSize;
            };
            SizeData size;

            struct DrawData
            {
                ftk::Box2I g;
                ftk::Box2I g2;
                ftk::Box2I labelGeometry;
                ftk::Box2I durationGeometry;
                ftk::TriMesh2F border;
                std::vector<std::shared_ptr<ftk::Glyph> > labelGlyphs;
                std::vector<std::shared_ptr<ftk::Glyph> > durationGlyphs;
            };
            std::optional<DrawData> draw;
        };

        void IBasicItem::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::string& label,
            const ftk::Color4F& color,
            const std::string& objectName,
            const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Item>& item,
            double scale,
            const ItemOptions& options,
            const DisplayOptions& displayOptions,
            const std::shared_ptr<ItemData>& itemData,
            const std::shared_ptr<ftk::IWidget>& parent)
        {
            OTIO_NS::TimeRange timeRange = invalidTimeRange;
            const auto timeRangeOpt = item->trimmed_range_in_parent();
            if (timeRangeOpt.has_value())
            {
                timeRange = timeRangeOpt.value();
            }
            const OTIO_NS::TimeRange availableRange = item->available_range();
            const OTIO_NS::TimeRange trimmedRange = item->trimmed_range();
            IItem::_init(
                context,
                objectName,
                timeRange,
                availableRange,
                trimmedRange,
                scale,
                options,
                displayOptions,
                itemData,
                parent);
            FTK_P();

            p.label = label;
            p.color = color;
            p.markers = getMarkers(item.value);

            _textUpdate();
        }

        IBasicItem::IBasicItem() :
            _p(new Private)
        {}

        IBasicItem::~IBasicItem()
        {}

        void IBasicItem::setScale(double value)
        {
            const bool changed = value != _scale;
            IItem::setScale(value);
            FTK_P();
            if (changed)
            {
                p.draw.reset();
            }
        }

        void IBasicItem::setDisplayOptions(const DisplayOptions& value)
        {
            const bool changed = value != _displayOptions;
            IItem::setDisplayOptions(value);
            FTK_P();
            if (changed)
            {
                _textUpdate();
            }
        }
        
        ftk::Size2I IBasicItem::getSizeHint() const
        {
            FTK_P();
            ftk::Size2I out;
            out.w = _timeRange.duration().rescaled_to(1.0).value() * _scale;
            if (!_displayOptions.minimize)
            {
                out.h +=
                    p.size.fontMetrics.lineHeight +
                    p.size.margin * 2;
            }
            out.h += p.size.border * 4;
            return out;
        }

        void IBasicItem::setGeometry(const ftk::Box2I& value)
        {
            if (value != getGeometry())
            {
                _p->draw.reset();
            }
            IItem::setGeometry(value);
        }
        
        void IBasicItem::styleEvent(const ftk::StyleEvent& event)
        {
            IItem::styleEvent(event);
            FTK_P();
            if (event.hasChanges())
            {
                p.size.init = true;
                p.draw.reset();
            }
        }

        void IBasicItem::sizeHintEvent(const ftk::SizeHintEvent& event)
        {
            IItem::sizeHintEvent(event);
            FTK_P();
            if (p.size.init)
            {
                p.size.init = false;
                p.size.margin = event.style->getSizeRole(ftk::SizeRole::MarginInside, event.displayScale);
                p.size.border = event.style->getSizeRole(ftk::SizeRole::Border, event.displayScale);
                p.size.fontInfo = event.style->getFont(ftk::FontType::Regular, event.displayScale);
                p.size.fontMetrics = event.fontSystem->getMetrics(p.size.fontInfo);
                p.size.labelSize = !_displayOptions.minimize ?
                    event.fontSystem->getSize(p.label, p.size.fontInfo) :
                    ftk::Size2I();
                p.size.durationSize = !_displayOptions.minimize ?
                    event.fontSystem->getSize(p.durationLabel, p.size.fontInfo) :
                    ftk::Size2I();
                p.draw.reset();
            }
        }

        void IBasicItem::clipEvent(const ftk::Box2I& clipRect, bool clipped)
        {
            IItem::clipEvent(clipRect, clipped);
            FTK_P();
            if (clipped)
            {
                p.draw.reset();
            }
        }

        void IBasicItem::drawEvent(
            const ftk::Box2I& drawRect,
            const ftk::DrawEvent& event)
        {
            IItem::drawEvent(drawRect, event);
            FTK_P();

            if (!p.draw.has_value())
            {
                p.draw = Private::DrawData();
                p.draw->g = getGeometry();
                p.draw->g2 = ftk::margin(p.draw->g, -(p.size.border * 2));
                if (p.draw->g2.w() < 1)
                {
                    // An item narrower than the border would draw nothing at
                    // all and show the track behind it, which reads as a gap
                    // rather than as a very short item. Keep a pixel of it.
                    p.draw->g2.min.x = p.draw->g.min.x;
                    p.draw->g2.max.x = p.draw->g.min.x;
                }
                p.draw->labelGeometry = ftk::Box2I(
                    p.draw->g2.min.x + p.size.margin,
                    p.draw->g2.min.y + p.size.margin,
                    p.size.labelSize.w,
                    p.size.fontMetrics.lineHeight);
                p.draw->durationGeometry = ftk::Box2I(
                    p.draw->g2.max.x -
                    p.size.durationSize.w -
                    p.size.margin,
                    p.draw->g2.min.y + p.size.margin,
                    p.size.durationSize.w,
                    p.size.fontMetrics.lineHeight);
                p.draw->border = ftk::border(p.draw->g, p.size.border * 2);
            }

            // Draw the selection border.
            ftk::ColorRole colorRole = getSelectRole();
            if (colorRole != ftk::ColorRole::None)
            {
                event.render->drawMesh(
                    p.draw->border,
                    event.style->getColorRole(colorRole));
            }

            // Draw the background.
            event.render->drawRect(
                p.draw->g2,
                isEnabled() ? p.color : ftk::greyscale(p.color));

            // Draw the labels.
            if (!_displayOptions.minimize)
            {
                std::unique_ptr<ftk::ClipRectEnabledState> clipRectEnabledState;
                std::unique_ptr<ftk::ClipRectState> clipRectState;
                if (!ftk::contains(p.draw->g2, p.draw->labelGeometry) ||
                    !ftk::contains(p.draw->g2, p.draw->durationGeometry))
                {
                    clipRectEnabledState.reset(new ftk::ClipRectEnabledState(event.render));
                    clipRectState.reset(new ftk::ClipRectState(event.render));
                    event.render->setClipRectEnabled(true);
                    event.render->setClipRect(ftk::intersect(p.draw->g2, drawRect));
                }

                const bool enabled = isEnabled();
                if (ftk::intersects(drawRect, p.draw->labelGeometry))
                {
                    if (!p.label.empty() && p.draw->labelGlyphs.empty())
                    {
                        p.draw->labelGlyphs = event.fontSystem->getGlyphs(p.label, p.size.fontInfo);
                    }
                    event.render->drawText(
                        p.draw->labelGlyphs,
                        p.size.fontMetrics,
                        p.draw->labelGeometry.min,
                        event.style->getColorRole(
                            enabled ?
                            ftk::ColorRole::Text :
                            ftk::ColorRole::TextDisabled));
                }

                if (ftk::intersects(drawRect, p.draw->durationGeometry) &&
                    !ftk::intersects(p.draw->durationGeometry, p.draw->labelGeometry))
                {
                    if (!p.durationLabel.empty() && p.draw->durationGlyphs.empty())
                    {
                        p.draw->durationGlyphs = event.fontSystem->getGlyphs(p.durationLabel, p.size.fontInfo);
                    }
                    event.render->drawText(
                        p.draw->durationGlyphs,
                        p.size.fontMetrics,
                        p.draw->durationGeometry.min,
                        event.style->getColorRole(
                            enabled ?
                            ftk::ColorRole::Text :
                            ftk::ColorRole::TextDisabled));
                }
            }
        }

        int IBasicItem::_getMargin() const
        {
            return _p->size.margin;
        }

        int IBasicItem::_getLineHeight() const
        {
            return _p->size.fontMetrics.lineHeight;
        }

        ftk::Box2I IBasicItem::_getInsideGeometry() const
        {
            const ftk::Box2I& g = getGeometry();
            return ftk::margin(g, -(_p->size.border * 2));
        }

        void IBasicItem::_timeUnitsUpdate()
        {
            IItem::_timeUnitsUpdate();
            _textUpdate();
        }

        void IBasicItem::_textUpdate()
        {
            FTK_P();
            p.durationLabel = _getDurationLabel(_timeRange.duration());
            p.size.init = true;
            setSizeUpdate();
            setDrawUpdate();
        }
    }
}

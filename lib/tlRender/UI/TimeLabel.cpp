// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UI/TimeLabel.h>

#include <tlRender/Timeline/TimeUnits.h>

#include <ftk/UI/DrawUtil.h>
#include <ftk/UI/LayoutUtil.h>

#include <optional>

namespace tl
{
    namespace ui
    {
        struct TimeLabel::Private
        {
            std::shared_ptr<TimeUnitsModel> timeUnitsModel;
            std::optional<OTIO_NS::RationalTime> value;
            std::string text;
            std::string format;
            ftk::SizeRole marginRole = ftk::SizeRole::None;
            ftk::FontType font = ftk::FontType::Mono;
            ftk::ColorRole segmentRole = ftk::ColorRole::None;
            std::array<bool, 4> roundedCorners = { true, true, true, true };

            struct SizeData
            {
                bool init = true;
                int margin = 0;
                int cornerRadius = 0;
                ftk::FontInfo fontInfo;
                ftk::FontMetrics fontMetrics;
                ftk::Size2I textSize;
                ftk::Size2I formatSize;
            };
            SizeData size;

            struct DrawData
            {
                std::vector<std::shared_ptr<ftk::Glyph> > glyphs;
            };
            std::optional<DrawData> draw;

            std::shared_ptr<ftk::Observer<TimeUnits> > timeUnitsObserver;
        };

        void TimeLabel::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<TimeUnitsModel>& timeUnitsModel,
            const std::shared_ptr<IWidget>& parent)
        {
            IWidget::_init(context, "tl::ui::TimeLabel", parent);
            FTK_P();

            setVAlign(ftk::VAlign::Center);

            p.timeUnitsModel = timeUnitsModel;
            if (!p.timeUnitsModel)
            {
                p.timeUnitsModel = TimeUnitsModel::create(context);
            }

            _textUpdate();

            p.timeUnitsObserver = ftk::Observer<TimeUnits>::create(
                p.timeUnitsModel->observeTimeUnits(),
                [this](TimeUnits)
                {
                    _textUpdate();
                });
        }

        TimeLabel::TimeLabel() :
            _p(new Private)
        {}

        TimeLabel::~TimeLabel()
        {}

        std::shared_ptr<TimeLabel> TimeLabel::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<TimeUnitsModel>& timeUnitsModel,
            const std::shared_ptr<IWidget>& parent)
        {
            auto out = std::shared_ptr<TimeLabel>(new TimeLabel);
            out->_init(context, timeUnitsModel, parent);
            return out;
        }

        const std::shared_ptr<TimeUnitsModel>& TimeLabel::getTimeUnitsModel() const
        {
            return _p->timeUnitsModel;
        }

        const std::optional<OTIO_NS::RationalTime>& TimeLabel::getValue() const
        {
            return _p->value;
        }

        void TimeLabel::setValue(const std::optional<OTIO_NS::RationalTime>& value)
        {
            FTK_P();
            if (compareExact(value, p.value))
                return;
            p.value = value;
            _textUpdate();
        }

        void TimeLabel::setMarginRole(ftk::SizeRole value)
        {
            FTK_P();
            if (value == p.marginRole)
                return;
            p.marginRole = value;
            p.size.init = true;
            setSizeUpdate();
            setDrawUpdate();
        }

        void TimeLabel::setFont(ftk::FontType value)
        {
            FTK_P();
            if (value == p.font)
                return;
            p.font = value;
            p.size.init = true;
            setSizeUpdate();
            setDrawUpdate();
        }
        
        bool TimeLabel::isSegment() const
        {
            return true;
        }

        void TimeLabel::setSegment(ftk::ColorRole background, const std::array<bool, 4>& value)
        {
            FTK_P();
            if (background == p.segmentRole && value == p.roundedCorners)
                return;
            p.segmentRole = background;
            p.roundedCorners = value;
            // The background spans the row like its neighbors; the text is
            // centered in it below.
            setVAlign(ftk::VAlign::Fill);
            setDrawUpdate();
        }

        ftk::Size2I TimeLabel::getSizeHint() const
        {
            FTK_P();
            ftk::Size2I out;
            out.w =
                std::max(p.size.textSize.w, p.size.formatSize.w) +
                p.size.margin * 2;
            out.h =
                p.size.fontMetrics.lineHeight +
                p.size.margin * 2;
            return out;
        }

        void TimeLabel::styleEvent(const ftk::StyleEvent& event)
        {
            FTK_P();
            if (event.hasChanges())
            {
                p.size.init = true;
                p.draw.reset();
            }
        }

        void TimeLabel::sizeHintEvent(const ftk::SizeHintEvent& event)
        {
            IWidget::sizeHintEvent(event);
            FTK_P();
            if (p.size.init)
            {
                p.size.init = false;
                p.size.margin = event.style->getSizeRole(p.marginRole, event.displayScale);
                p.size.cornerRadius = event.style->getSizeRole(ftk::SizeRole::CornerRadius, event.displayScale);
                p.size.fontInfo = event.style->getFont(p.font, event.displayScale);
                p.size.fontMetrics = event.fontSystem->getMetrics(p.size.fontInfo);
                p.size.textSize = event.fontSystem->getSize(p.text, p.size.fontInfo);
                p.size.formatSize = event.fontSystem->getSize(p.format, p.size.fontInfo);
                p.draw.reset();
            }
        }

        void TimeLabel::clipEvent(const ftk::Box2I& clipRect, bool clipped)
        {
            IWidget::clipEvent(clipRect, clipped);
            FTK_P();
            if (clipped)
            {
                p.draw.reset();
            }
        }

        void TimeLabel::drawEvent(
            const ftk::Box2I& drawRect,
            const ftk::DrawEvent& event)
        {
            IWidget::drawEvent(drawRect, event);
            FTK_P();

            if (!p.draw.has_value())
            {
                p.draw = Private::DrawData();
            }

            if (p.segmentRole != ftk::ColorRole::None)
            {
                const int r = p.size.cornerRadius;
                event.render->drawMesh(
                    ftk::rect(
                        getGeometry(),
                        {
                            p.roundedCorners[0] ? r : 0,
                            p.roundedCorners[1] ? r : 0,
                            p.roundedCorners[2] ? r : 0,
                            p.roundedCorners[3] ? r : 0
                        }),
                    event.style->getColorRole(p.segmentRole));
            }

            const ftk::Box2I g = ftk::margin(
                align(
                    getGeometry(),
                    getSizeHint(),
                    getHAlign(),
                    p.segmentRole != ftk::ColorRole::None ?
                    ftk::VAlign::Center :
                    getVAlign()),
                -p.size.margin);

            if (!p.text.empty() && p.draw->glyphs.empty())
            {
                p.draw->glyphs = event.fontSystem->getGlyphs(p.text, p.size.fontInfo);
            }
            event.render->drawText(
                p.draw->glyphs,
                p.size.fontMetrics,
                g.min,
                event.style->getColorRole(
                    isEnabled() ?
                    ftk::ColorRole::Text :
                    ftk::ColorRole::TextDisabled));
        }

        void TimeLabel::_textUpdate()
        {
            FTK_P();
            p.text = std::string();
            p.format = std::string();
            if (p.timeUnitsModel)
            {
                const TimeUnits timeUnits = p.timeUnitsModel->getTimeUnits();
                p.text = timeToText(p.value, timeUnits);
                p.format = formatString(timeUnits);
            }
            p.size.init = true;
            setSizeUpdate();
            setDrawUpdate();
        }
    }
}

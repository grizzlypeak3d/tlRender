// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/UI/Export.h>
#include <tlRender/Core/Export.h>

#include <ftk/UI/IWidget.h>

#include <opentimelineio/version.h>

namespace tl
{
    class TimeUnitsModel;

    namespace ui
    {
        //! Time label.
        class TL_UI_API_TYPE TimeLabel : public ftk::IWidget
        {
            FTK_NON_COPYABLE(TimeLabel);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<TimeUnitsModel>&,
                const std::shared_ptr<IWidget>& parent);

            TimeLabel();

        public:
            TL_UI_API virtual ~TimeLabel();

            //! Create a new widget.
            TL_UI_API static std::shared_ptr<TimeLabel> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<TimeUnitsModel>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

            //! Get the time units model.
            TL_UI_API const std::shared_ptr<TimeUnitsModel>& getTimeUnitsModel() const;

            //! Get the time value, unset when the label has none.
            TL_UI_API const std::optional<OTIO_NS::RationalTime>& getValue() const;

            //! Set the time value.
            TL_UI_API void setValue(const std::optional<OTIO_NS::RationalTime>&);

            //! Set the margin role.
            TL_UI_API void setMarginRole(ftk::SizeRole);

            //! Set the font 
            TL_UI_API void setFont(ftk::FontType);

            TL_UI_API ftk::Size2I getSizeHint() const override;
            TL_UI_API void styleEvent(const ftk::StyleEvent&) override;
            TL_UI_API void sizeHintEvent(const ftk::SizeHintEvent&) override;
            TL_UI_API void clipEvent(const ftk::Box2I&, bool) override;
            TL_UI_API void drawEvent(const ftk::Box2I&, const ftk::DrawEvent&) override;

        private:
            void _textUpdate();

            FTK_PRIVATE();
        };
    }
}

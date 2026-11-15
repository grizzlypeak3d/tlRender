// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/UI/Export.h>
#include <tlRender/Core/Export.h>

#include <ftk/UI/IContainer.h>
#include <ftk/UI/IWidget.h>

#include <opentimelineio/version.h>

#include <optional>

namespace tl
{
    class TimeUnitsModel;

    namespace ui
    {
        //! Mapping between the time a player runs on and the time to show.
        //!
        //! These are not always the same time. A sequence read with the frames
        //! it is missing left out is played at the length of the frames it
        //! has, so counting from the start does not name the frame; the
        //! media's own time does. Left unset the two are the same.
        struct TL_UI_API_TYPE TimeMap
        {
            std::function<OTIO_NS::RationalTime(const OTIO_NS::RationalTime&)> toMedia;
            std::function<OTIO_NS::RationalTime(const OTIO_NS::RationalTime&)> fromMedia;
        };

        //! Time value editor.
        class TL_UI_API_TYPE TimeEdit : public ftk::IContainer
        {
            FTK_NON_COPYABLE(TimeEdit);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<TimeUnitsModel>&,
                const std::shared_ptr<IWidget>& parent);

            TimeEdit();

        public:
            TL_UI_API virtual ~TimeEdit();

            //! Create a new widget.
            TL_UI_API static std::shared_ptr<TimeEdit> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<TimeUnitsModel>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

            //! Get the time units model.
            TL_UI_API const std::shared_ptr<TimeUnitsModel>& getTimeUnitsModel() const;

            //! Get the time value, unset when the edit has none.
            TL_UI_API const std::optional<OTIO_NS::RationalTime>& getValue() const;

            //! Set the time value.
            TL_UI_API void setValue(const std::optional<OTIO_NS::RationalTime>&);

            //! Set the mapping between the value and the time shown. The
            //! value, and the callback, stay in the player's time.
            TL_UI_API void setTimeMap(const TimeMap&);

            //! Set the times the value can take, in the player's time, so the
            //! edit is wide enough for any of them from the start rather than
            //! growing as the value crosses a digit. The edit is always wide
            //! enough for the value it shows.
            TL_UI_API void setRange(const std::optional<OTIO_NS::TimeRange>&);

            //! Set the time value callback.
            TL_UI_API void setCallback(const std::function<void(const OTIO_NS::RationalTime&)>&);

            //! Set a callback for the edit taking or losing the key focus:
            //! for an editor of several fields that ends when the focus
            //! leaves all of them.
            TL_UI_API void setFocusCallback(const std::function<void(bool)>&);

            //! Set whether taking the key focus selects the text, as it does
            //! by default: not for a focus given back, after the edit was
            //! made again.
            TL_UI_API void setSelectAllOnFocus(bool);

            //! Select all.
            TL_UI_API void selectAll();

            //! Set the font.
            TL_UI_API void setFont(ftk::FontType);

            //! Set whether the increment buttons are visible; too
            //! small for touch, and redundant beside frame controls.
            TL_UI_API void setIncButtonsVisible(bool);

            //! Set the border color role.
            TL_UI_API void setBorderRole(ftk::ColorRole);

            //! Set the well color role.
            TL_UI_API void setWellRole(ftk::ColorRole);

            TL_UI_API void takeKeyFocus() override;
            TL_UI_API void keyPressEvent(ftk::KeyEvent&) override;
            TL_UI_API void keyReleaseEvent(ftk::KeyEvent&) override;

        private:
            std::optional<OTIO_NS::RationalTime> _mediaValue() const;
            std::optional<OTIO_NS::RationalTime> _toShown(
                const std::optional<OTIO_NS::RationalTime>&) const;
            void _commitValue(const std::string&);
            void _commitValue(const OTIO_NS::RationalTime&);
            void _textUpdate();

            FTK_PRIVATE();
        };
    }
}

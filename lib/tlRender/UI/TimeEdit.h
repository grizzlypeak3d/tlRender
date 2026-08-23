// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/UI/Export.h>
#include <tlRender/Core/Export.h>

#include <ftk/UI/IContainer.h>
#include <ftk/UI/IWidget.h>

#include <opentimelineio/version.h>

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

            //! Set the time value callback.
            TL_UI_API void setCallback(const std::function<void(const OTIO_NS::RationalTime&)>&);

            //! Select all.
            TL_UI_API void selectAll();

            //! Set the font.
            TL_UI_API void setFont(ftk::FontType);

            TL_UI_API void takeKeyFocus() override;
            TL_UI_API void keyPressEvent(ftk::KeyEvent&) override;
            TL_UI_API void keyReleaseEvent(ftk::KeyEvent&) override;

        private:
            std::optional<OTIO_NS::RationalTime> _mediaValue() const;
            void _commitValue(const std::string&);
            void _commitValue(const OTIO_NS::RationalTime&);
            void _textUpdate();

            FTK_PRIVATE();
        };
    }
}

// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/UI/Export.h>
#include <tlRender/Timeline/TimeUnits.h>

#include <ftk/UI/IContainer.h>
#include <ftk/UI/IWidget.h>

namespace tl
{
    namespace ui
    {
        //! Time units widget.
        class TL_UI_API_TYPE TimeUnitsWidget : public ftk::IContainer
        {
            FTK_NON_COPYABLE(TimeUnitsWidget);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<TimeUnitsModel>&,
                const std::shared_ptr<IWidget>& parent);

            TimeUnitsWidget();

        public:
            TL_UI_API virtual ~TimeUnitsWidget();

            //! Create a new widget.
            TL_UI_API static std::shared_ptr<TimeUnitsWidget> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<TimeUnitsModel>&,
                const std::shared_ptr<IWidget>& parent = nullptr);


        private:
            FTK_PRIVATE();
        };
    }
}

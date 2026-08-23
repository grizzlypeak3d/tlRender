// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Player.h>

#include <ftk/UI/ToolBar.h>

namespace tl
{
    namespace ui
    {
        //! Frame tool bar.
        class TL_API_TYPE FrameToolBar : public ftk::ToolBar
        {
            FTK_NON_COPYABLE(FrameToolBar);

        protected:
            TL_API void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<IWidget>& parent);

            TL_API FrameToolBar();

        public:
            TL_API virtual ~FrameToolBar();

            //! Create a new widget.
            TL_API static std::shared_ptr<FrameToolBar> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<IWidget>& parent = nullptr);
            
            //! Get the actions.
            TL_API const std::map<std::string, std::shared_ptr<ftk::Action> >& getActions() const;

            //! Get the player.
            TL_API const std::shared_ptr<Player>& getPlayer() const;

            //! Set the player.
            TL_API void setPlayer(const std::shared_ptr<Player>&);

        private:
            void _widgetUpdate();

            FTK_PRIVATE();
        };
    }
}

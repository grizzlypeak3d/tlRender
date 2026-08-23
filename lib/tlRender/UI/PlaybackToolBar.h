// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Player.h>

#include <ftk/UI/ToolBar.h>

namespace tl
{
    namespace ui
    {
        //! Playback tool bar.
        class TL_API_TYPE PlaybackToolBar : public ftk::ToolBar
        {
            FTK_NON_COPYABLE(PlaybackToolBar);

        protected:
            TL_API void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<IWidget>& parent);

            TL_API PlaybackToolBar();

        public:
            TL_API virtual ~PlaybackToolBar();

            //! Create a new widget.
            TL_API static std::shared_ptr<PlaybackToolBar> create(
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

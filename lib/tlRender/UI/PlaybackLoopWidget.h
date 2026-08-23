// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/UI/Export.h>
#include <tlRender/Timeline/Player.h>

#include <ftk/UI/IContainer.h>
#include <ftk/UI/IWidget.h>

namespace tl
{
    namespace ui
    {
        //! Time label.
        class TL_UI_API_TYPE PlaybackLoopWidget : public ftk::IContainer
        {
            FTK_NON_COPYABLE(PlaybackLoopWidget);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<IWidget>& parent);

            PlaybackLoopWidget();

        public:
            TL_UI_API virtual ~PlaybackLoopWidget();

            //! Create a new widget.
            TL_UI_API static std::shared_ptr<PlaybackLoopWidget> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

            //! Get the playback loop.
            TL_UI_API Loop getLoop() const;

            //! Set the playback loop.
            TL_UI_API void setLoop(Loop);

            //! Set the callback.
            TL_UI_API void setCallback(const std::function<void(Loop)>&);


        private:
            void _widgetUpdate();
            void _showPopup();

            FTK_PRIVATE();
        };
    }
}

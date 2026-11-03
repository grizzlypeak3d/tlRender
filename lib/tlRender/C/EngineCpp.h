// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

//! \file EngineCpp.h
//! The C++ side of the C interface, for C++ built beside it: another
//! interface over models, say, which sets what the view draws with from
//! the models and needs the engine's objects to do it. Not for Swift,
//! which sees Engine.h alone.

#include <tlRender/C/Engine.h>

#include <tlRender/Timeline/BackgroundOptions.h>
#include <tlRender/Timeline/ColorOptions.h>
#include <tlRender/Timeline/DisplayOptions.h>
#include <tlRender/Timeline/ForegroundOptions.h>
#include <tlRender/Timeline/Player.h>
#include <tlRender/Timeline/Timeline.h>

#include <ftk/Core/Context.h>
#include <ftk/Core/RenderOptions.h>

#include <functional>
#include <memory>

namespace tl
{
    namespace c
    {
        //! The engine's context.
        TL_C_API std::shared_ptr<ftk::Context> getContext(tl_engine*);

        //! A player's timeline and player.
        TL_C_API std::shared_ptr<Timeline> getTimeline(tl_player*);
        TL_C_API std::shared_ptr<Player> getPlayer(tl_player*);

        //! What a view draws the picture with. Each is kept by the view
        //! and applied on its next draw.
        TL_C_API void setOCIOOptions(tl_view*, const OCIOOptions&);
        TL_C_API void setOCIOInputResolver(
            tl_view*,
            const std::function<std::string(const std::string& path, const ftk::ImageTags&)>&);
        TL_C_API void setLUTOptions(tl_view*, const LUTOptions&);
        TL_C_API void setImageOptions(tl_view*, const ftk::ImageOptions&);
        TL_C_API void setDisplayOptions(tl_view*, const DisplayOptions&);
        TL_C_API void setBackgroundOptions(tl_view*, const BackgroundOptions&);
        TL_C_API void setForegroundOptions(tl_view*, const ForegroundOptions&);
    }
}

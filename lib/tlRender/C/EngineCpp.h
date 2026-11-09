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
#include <tlRender/Timeline/CompareOptions.h>
#include <tlRender/Timeline/ForegroundOptions.h>
#include <tlRender/Timeline/Player.h>
#include <tlRender/Timeline/Timeline.h>

#include <ftk/Core/Box.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/IRender.h>
#include <ftk/Core/RenderOptions.h>
#include <ftk/Core/Size.h>

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

        //! What the thumbnails and posters are shown with: the color the
        //! picture is drawn with, as the views have it, given to the
        //! engine as well so that a library with no view open has it.
        //! Put on each one as it is collected, by
        //! tl_player_get_thumbnail() and tl_engine_get_poster(), with
        //! the input of the media the thumbnail was made from.
        TL_C_API void setThumbnailColor(
            tl_engine*,
            const OCIOOptions&,
            const std::function<std::string(const std::string& path, const ftk::ImageTags&)>&,
            const LUTOptions&);

        //! How the player's timeline and the one it is compared with are
        //! drawn; see tl_player_set_compare().
        TL_C_API void setCompareOptions(tl_view*, const CompareOptions&);

        //! Where the last draw put the picture, in the view's pixels: the
        //! render space at a position and a zoom, as the desktop viewport
        //! has it, the picture's box in that space, and the size of the
        //! image of the frame shown. Empty sizes before the first draw.
        struct ViewPicture
        {
            ftk::V2F    pos;
            float       zoom = 1.F;
            ftk::Box2I  box;
            ftk::Size2I imageSize;
            float       displayScale = 1.F;
        };
        TL_C_API ViewPicture getViewPicture(tl_view*);

        //! Something drawn over the picture and under the widgets, in the
        //! view's pixels: annotations, say. Called during the draw.
        TL_C_API void setOverlay(
            tl_view*,
            const std::function<void(const std::shared_ptr<ftk::IRender>&)>&);

        //! Draw again at the next chance: what the overlay draws changed.
        TL_C_API void setNeedsDraw(tl_view*);
    }
}

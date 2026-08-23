// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Export.h>
#include <tlRender/Core/Util.h>

#include <ftk/Core/Util.h>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace tl
{
    //! Transitions.
    enum class TL_TIMELINE_API_TYPE Transition
    {
        None,
        Dissolve,

        Count,
        First = None
    };
    FTK_ENUM(TL_TIMELINE_API, Transition);

    //! Convert to a transition.
    TL_TIMELINE_API Transition toTransition(const std::string&);
}

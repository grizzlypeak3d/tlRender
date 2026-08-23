// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/Transition.h>

#include <tlRender/Core/Time.h>

#include <ftk/Core/Error.h>
#include <ftk/Core/String.h>

#include <opentimelineio/transition.h>

#include <array>
#include <sstream>

namespace tl
{
    FTK_ENUM_IMPL(
        Transition,
        "None",
        "Dissolve");

    Transition toTransition(const std::string& value)
    {
        Transition out = Transition::None;
        if (OTIO_NS::Transition::Type::SMPTE_Dissolve == value)
        {
            out = Transition::Dissolve;
        }
        return out;
    }
}

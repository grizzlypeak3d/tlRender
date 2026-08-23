// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Export.h>
#include <tlRender/Core/Export.h>

#include <memory>

namespace ftk
{
    class Context;
}

namespace tl
{
    //! Initialize the library.
    TL_TIMELINE_API void init(const std::shared_ptr<ftk::Context>&);
}

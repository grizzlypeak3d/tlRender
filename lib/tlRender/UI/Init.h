// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/UI/Export.h>
#include <tlRender/Core/Export.h>

#include <memory>
#include <string>

namespace ftk
{
    class Context;
}

namespace tl
{
    //! Timeline user interface
    namespace ui
    {
        //! Initialize the library.
        TL_UI_API void init(const std::shared_ptr<ftk::Context>&);
    }
}

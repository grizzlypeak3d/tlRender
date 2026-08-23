// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Core/Export.h>

#include <ftk/Core/Util.h>

#include <string>

namespace tl
{
    //! Get the URL scheme.
    TL_CORE_API std::string getURLScheme(const std::string&);

    //! Encode a URL.
    TL_CORE_API std::string encodeURL(const std::string&);

    //! Decode a URL.
    TL_CORE_API std::string decodeURL(const std::string&);
}

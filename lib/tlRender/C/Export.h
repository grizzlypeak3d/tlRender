// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

// The C interface's own export macro: it is a library of its own, and the
// timeline library's would read its functions as imported while they are
// being defined. C as well as C++.
#include <tlRender/Core/Export.h>

#if defined(TL_C_STATIC)
#    define TL_C_API
#elif defined(TL_C_EXPORTS)
#    define TL_C_API TLRENDER_EXPORT
#else
#    define TL_C_API TLRENDER_IMPORT
#endif

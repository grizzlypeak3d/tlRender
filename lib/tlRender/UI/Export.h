// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

// The platform layer -- TLRENDER_EXPORT and the rest -- is shared, and lives
// with the core library.
#include <tlRender/Core/Export.h>

// The macros for the user interface library. Each library in this project has its own set: one
// shared between them would be defined for whichever library is being built,
// so a library compiling a sibling's headers would read the sibling's API as
// dllexport where it wants dllimport. Functions survive that -- the linker
// takes them from the import library -- and data does not.
#if defined(TL_UI_STATIC)
#    define TL_UI_API
#    define TL_UI_API_TYPE
#    define TL_UI_API_TEMPLATE_CLASS(...)
#    define TL_UI_API_TEMPLATE_STRUCT(...)
#    define TL_UI_LOCAL
#else
#    if defined(TL_UI_EXPORTS)
#        define TL_UI_API TLRENDER_EXPORT
#        define TL_UI_API_TYPE TLRENDER_EXPORT_TYPE
#        define TL_UI_API_TEMPLATE_CLASS(...)                                             TLRENDER_EXPORT_TEMPLATE(class, __VA_ARGS__)
#        define TL_UI_API_TEMPLATE_STRUCT(...)                                            TLRENDER_EXPORT_TEMPLATE(struct, __VA_ARGS__)
#    else
#        define TL_UI_API TLRENDER_IMPORT
#        define TL_UI_API_TYPE TLRENDER_IMPORT_TYPE
#        define TL_UI_API_TEMPLATE_CLASS(...)                                             TLRENDER_IMPORT_TEMPLATE(class, __VA_ARGS__)
#        define TL_UI_API_TEMPLATE_STRUCT(...)                                            TLRENDER_IMPORT_TEMPLATE(struct, __VA_ARGS__)
#    endif
#    define TL_UI_LOCAL TLRENDER_HIDDEN
#endif

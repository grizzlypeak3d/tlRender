// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

// For an explanation of how these export defines work, see:
// https://github.com/PixarAnimationStudios/OpenUSD/blob/dev/pxr/base/arch/export.h
#if defined(_WINDOWS)
#    if defined(__GNUC__) && __GNUC__ >= 4 || defined(__clang__)
#        define TLRENDER_EXPORT __attribute__((dllexport))
#        define TLRENDER_IMPORT __attribute__((dllimport))
#        define TLRENDER_HIDDEN
#        define TLRENDER_EXPORT_TYPE
#        define TLRENDER_IMPORT_TYPE
#    else
#        define TLRENDER_EXPORT __declspec(dllexport)
#        define TLRENDER_IMPORT __declspec(dllimport)
#        define TLRENDER_HIDDEN
#        define TLRENDER_EXPORT_TYPE
#        define TLRENDER_IMPORT_TYPE
#    endif
#elif defined(__GNUC__) && __GNUC__ >= 4 || defined(__clang__)
#    define TLRENDER_EXPORT __attribute__((visibility("default")))
#    define TLRENDER_IMPORT
#    define TLRENDER_HIDDEN __attribute__((visibility("hidden")))
#    if defined(__clang__)
#        define TLRENDER_EXPORT_TYPE                                     \
            __attribute__((type_visibility("default")))
#    else
#        define TLRENDER_EXPORT_TYPE                                     \
            __attribute__((visibility("default")))
#    endif
#    define TLRENDER_IMPORT_TYPE
#else
#    define TLRENDER_EXPORT
#    define TLRENDER_IMPORT
#    define TLRENDER_HIDDEN
#    define TLRENDER_EXPORT_TYPE
#    define TLRENDER_IMPORT_TYPE
#endif
#define TLRENDER_EXPORT_TEMPLATE(type, ...)
#define TLRENDER_IMPORT_TEMPLATE(type, ...)                              \
    extern template type TLRENDER_IMPORT __VA_ARGS__

// The macros for the core library. Each library in this project has its own set: one
// shared between them would be defined for whichever library is being built,
// so a library compiling a sibling's headers would read the sibling's API as
// dllexport where it wants dllimport. Functions survive that -- the linker
// takes them from the import library -- and data does not.
#if defined(TL_CORE_STATIC)
#    define TL_CORE_API
#    define TL_CORE_API_TYPE
#    define TL_CORE_API_TEMPLATE_CLASS(...)
#    define TL_CORE_API_TEMPLATE_STRUCT(...)
#    define TL_CORE_LOCAL
#else
#    if defined(TL_CORE_EXPORTS)
#        define TL_CORE_API TLRENDER_EXPORT
#        define TL_CORE_API_TYPE TLRENDER_EXPORT_TYPE
#        define TL_CORE_API_TEMPLATE_CLASS(...)                                             TLRENDER_EXPORT_TEMPLATE(class, __VA_ARGS__)
#        define TL_CORE_API_TEMPLATE_STRUCT(...)                                            TLRENDER_EXPORT_TEMPLATE(struct, __VA_ARGS__)
#    else
#        define TL_CORE_API TLRENDER_IMPORT
#        define TL_CORE_API_TYPE TLRENDER_IMPORT_TYPE
#        define TL_CORE_API_TEMPLATE_CLASS(...)                                             TLRENDER_IMPORT_TEMPLATE(class, __VA_ARGS__)
#        define TL_CORE_API_TEMPLATE_STRUCT(...)                                            TLRENDER_IMPORT_TEMPLATE(struct, __VA_ARGS__)
#    endif
#    define TL_CORE_LOCAL TLRENDER_HIDDEN
#endif

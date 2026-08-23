// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Core/Export.h>

//! Convenience macro for enum utilities.
//! 
//! Required includes:
//! * iostream
//! * string
//! * vector
#define TL_ENUM(ENUM) \
    TL_CORE_API std::vector<ENUM> get##ENUM##Enums(); \
    TL_CORE_API std::vector<std::string> get##ENUM##Labels(); \
    TL_CORE_API std::string getLabel(ENUM); \
    TL_CORE_API std::string to_string(ENUM); \
    TL_CORE_API bool from_string(const std::string&, ENUM&); \
    TL_CORE_API std::ostream& operator << (std::ostream&, ENUM)

//! Implementation macro for enum utilities.
//! 
//! Required includes:
//! * ftk/core/Error.h
//! * ftk/core/String.h
//! * algorithm
//! * array
//! * sstream
#define TL_ENUM_IMPL(ENUM, ...) \
    FTK_ENUM_IMPL(ENUM, __VA_ARGS__)


// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <opentimelineio/version.h>

//! The OpenTimelineIO version as one number: major * 10000 + minor * 100
//! + patch. Releases up to 0.17 define no numeric version macros at all,
//! and read as zero.
#if defined(OPENTIMELINEIO_VERSION_MAJOR)
#define TLRENDER_OTIO_VERSION \
    (OPENTIMELINEIO_VERSION_MAJOR * 10000 + \
     OPENTIMELINEIO_VERSION_MINOR * 100 + \
     OPENTIMELINEIO_VERSION_PATCH)
#else // OPENTIMELINEIO_VERSION_MAJOR
#define TLRENDER_OTIO_VERSION 0
#endif // OPENTIMELINEIO_VERSION_MAJOR

//! Items carry an RGBA color from 0.18.
#define TLRENDER_OTIO_ITEM_COLOR (TLRENDER_OTIO_VERSION >= 1800)

//! Markers carry an RGBA color from 0.19; before that a named color.
#define TLRENDER_OTIO_MARKER_COLOR (TLRENDER_OTIO_VERSION >= 1900)

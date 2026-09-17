// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the feather-tk project.

#include <tlRender/UIPy/Bindings.h>
#include <tlRender/TimelinePy/Bindings.h>
#include <tlRender/IOPy/Bindings.h>
#include <tlRender/CorePy/Bindings.h>

#include <tlRender/Core/Version.h>

#include <opentimelineio/version.h>

#include <tlRender/TimelinePy/OTIOCasters.h>

#include <nanobind/nanobind.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>

#include <iostream>

namespace nb = nanobind;

NB_MODULE(_tlrender, m)
{
    m.doc() = "tlRender is an open source library for building playback and review applications for visual effects, film, and animation.";

    nb::module_::import_("opentimelineio");
    nb::module_::import_("feather_tk");

    m.attr("VERSION_MAJOR") = TLRENDER_VERSION_MAJOR;
    m.attr("VERSION_MINOR") = TLRENDER_VERSION_MINOR;
    m.attr("VERSION_PATCH") = TLRENDER_VERSION_PATCH;
    m.attr("VERSION_DEV") = TLRENDER_VERSION_DEV;
    m.attr("VERSION_FULL") = TLRENDER_VERSION_FULL;

    tl::python::coreBind(m);
    tl::python::ioBind(m);
    tl::python::timelineBind(m);
    tl::python::uiBind(m);
}


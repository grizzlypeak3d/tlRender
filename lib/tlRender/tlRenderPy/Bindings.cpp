// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the feather-tk project.

#include <tlRender/UIPy/Bindings.h>
#include <tlRender/TimelinePy/Bindings.h>
#include <tlRender/IOPy/Bindings.h>
#include <tlRender/CorePy/Bindings.h>

#include <opentimelineio/version.h>

#include <tlRender/TimelinePy/OTIOCasters.h>

#include <nanobind/nanobind.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>

#include <iostream>

namespace nb = nanobind;

NB_MODULE(tlRenderPy, m)
{
    m.doc() = "tlRender is an open source library for building playback and review applications for visual effects, film, and animation.";

    nb::module_::import_("opentimelineio");
    nb::module_::import_("ftkPy");

    tl::python::coreBind(m);
    tl::python::ioBind(m);
    tl::python::timelineBind(m);
    tl::python::uiBind(m);
}


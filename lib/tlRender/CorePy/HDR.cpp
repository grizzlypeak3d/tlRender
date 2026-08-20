// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/CorePy/Bindings.h>

#include <tlRender/Core/HDR.h>

#include <ftk/CorePy/Bindings.h>

#include <pybind11/operators.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void hdr(py::module_& m)
        {
            py::enum_<HDR_EOTF>(m, "HDR_EOTF")
                .value("SDR", HDR_EOTF::SDR)
                .value("HDR", HDR_EOTF::HDR)
                .value("ST2084", HDR_EOTF::ST2084);
            FTK_ENUM_BIND(m, HDR_EOTF);

            py::enum_<HDRPrimaries>(m, "HDRPrimaries")
                .value("Red", HDRPrimaries::Red)
                .value("Green", HDRPrimaries::Green)
                .value("Blue", HDRPrimaries::Blue)
                .value("White", HDRPrimaries::White);
            FTK_ENUM_BIND(m, HDRPrimaries);

            py::class_<HDRData>(m, "HDRData")
                .def(py::init())
                .def_readwrite("eotf", &HDRData::eotf)
                .def_readwrite("primaries", &HDRData::primaries)
                .def_readwrite("displayMasteringLuminance", &HDRData::displayMasteringLuminance)
                .def_readwrite("maxCLL", &HDRData::maxCLL)
                .def_readwrite("maxFALL", &HDRData::maxFALL)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);
        }
    }
}

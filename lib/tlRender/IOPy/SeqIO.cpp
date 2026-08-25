// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOPy/Bindings.h>

#include <tlRender/IO/SeqIO.h>

#include <ftk/CorePy/Bindings.h>

#include <pybind11/operators.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void seqIO(py::module_& m)
        {
            py::enum_<MissingFrames>(m, "MissingFrames")
                .value("Error", MissingFrames::Error)
                .value("Hold", MissingFrames::Hold)
                .value("Black", MissingFrames::Black)
                .value("Skip", MissingFrames::Skip)
                .value("Gaps", MissingFrames::Gaps);
            FTK_ENUM_BIND(m, MissingFrames);

            m.def("isStructural", &isStructural, py::arg("missingFrames"));

            py::class_<SeqOptions>(m, "SeqOptions")
                .def(py::init())
                .def_readwrite("defaultSpeed", &SeqOptions::defaultSpeed)
                .def_readwrite("missingFrames", &SeqOptions::missingFrames)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);
        }
    }
}

#include <pybind11/eigen.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "Logger.h"

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;
using namespace IG;

void scene_module(py::module_& m);   // Defined in scene.cpp
void runtime_module(py::module_& m); // Defined in runtime.cpp

PYBIND11_MODULE(pyignis, m)
{
    m.doc() = R"pbdoc(
        Ignis python plugin
        -----------------------
        .. currentmodule:: pyignis
        .. autosummary::
           :toctree: _generate
    )pbdoc";

    m.attr("__version__") = MACRO_STRINGIFY(IGNIS_VERSION);

    // Logger IO stuff
    m.def("setQuiet", [](bool b) { IG_LOGGER.setQuiet(b); });
    m.def("setVerbose", [](bool b) { IG_LOGGER.setVerbosity(b ? L_DEBUG : L_INFO); });

    scene_module(m);
    runtime_module(m);
}
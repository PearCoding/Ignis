#include <nanobind/nanobind.h>

#include "Logger.h"

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace nb = nanobind;
using namespace nb::literals;

using namespace IG;

void scene_module(nb::module_& m);   // Defined in scene.cpp
void runtime_module(nb::module_& m); // Defined in runtime.cpp

NB_MODULE(pyignis, m)
{
    m.doc() = "Ignis python interface";
    m.attr("__version__") = MACRO_STRINGIFY(IGNIS_VERSION);

    nb::enum_<LogLevel>(m, "LogLevel", "Enum holding verbosity level for logging")
        .value("Debug", LogLevel::L_DEBUG)
        .value("Info", LogLevel::L_INFO)
        .value("Warning", LogLevel::L_WARNING)
        .value("Error", LogLevel::L_ERROR)
        .value("Fatal", LogLevel::L_FATAL);

    // Logger IO stuff
    m.def("setQuiet", [](bool b) { IG_LOGGER.setQuiet(b); }, "Set True to disable all messages from the framework");
    m.def("setVerbose", [](bool b) { IG_LOGGER.setVerbosity(b ? L_DEBUG : L_INFO); }, "Set True to enable all messages from the framework, else only important messages will be shown. Shortcut for setVerbose(LogLevel.Debug)");
    m.def("setVerbose", [](LogLevel level) { IG_LOGGER.setVerbosity(level); }, "Set the level of verbosity for the logger");

    scene_module(m);
    runtime_module(m);
}
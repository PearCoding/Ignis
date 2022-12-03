#pragma once

#include "LoaderContext.h"

#include <optional>

namespace IG {

class Loader {
public:
    static std::optional<LoaderContext> load(const LoaderOptions& opts);

    /// Get a list of all available techniques
    static std::vector<std::string> getAvailableTechniqueTypes();

    /// Get a list of all available cameras
    static std::vector<std::string> getAvailableCameraTypes();
};
} // namespace IG
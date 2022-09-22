#pragma once

#include "CameraOrientation.h"
#include "TechniqueInfo.h"
#include "device/Target.h"
#include "table/SceneDatabase.h"

namespace IG {
constexpr size_t DefaultAlignment = sizeof(float) * 4;

struct LoaderResult {
    SceneDatabase Database;
    std::vector<TechniqueVariant> TechniqueVariants;
    std::vector<std::string> ResourceMap;
    IG::TechniqueInfo TechniqueInfo;
    IG::CameraOrientation CameraOrientation;
};
} // namespace IG
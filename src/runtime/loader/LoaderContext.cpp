#include "LoaderContext.h"
#include "Image.h"
#include "Logger.h"
#include "serialization/VectorSerializer.h"
#include "table/SceneDatabase.h"

// Include for constructor & deconstructor
#include "LoaderBSDF.h"
#include "LoaderCamera.h"
#include "LoaderEntity.h"
#include "LoaderLight.h"
#include "LoaderMedium.h"
#include "LoaderShape.h"
#include "LoaderTechnique.h"
#include "LoaderTexture.h"

namespace IG {

LoaderContext::LoaderContext()                           = default;
LoaderContext::LoaderContext(LoaderContext&&)            = default;
LoaderContext& LoaderContext::operator=(LoaderContext&&) = default;
LoaderContext::~LoaderContext()                          = default;

std::filesystem::path LoaderContext::handlePath(const std::filesystem::path& path, const SceneObject& obj) const
{
    if (path.is_absolute())
        return path;

    if (!obj.baseDir().empty()) {
        const auto p = obj.baseDir() / path;
        if (std::filesystem::exists(p))
            return std::filesystem::canonical(p);
    }

    if (!Options.FilePath.empty()) {
        const auto p = Options.FilePath.parent_path() / path;
        if (std::filesystem::exists(p))
            return std::filesystem::canonical(p);
    }

    return std::filesystem::canonical(path);
}

} // namespace IG
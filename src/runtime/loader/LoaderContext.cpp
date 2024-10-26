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

LoaderContext LoaderContext::copyForBake() const
{
    LoaderContext ctx;
    ctx.Options      = Options;
    ctx.Textures     = Textures;
    ctx.Cache        = Cache;
    ctx.CacheManager = CacheManager;

    // TODO: Maybe copy more?
    return ctx;
}

Path LoaderContext::getPath(SceneObject& obj, const std::string& propertyName) const
{
    return handlePath(obj, obj.property(propertyName).getString());
}

Path LoaderContext::handlePath(const SceneObject& obj, const std::string& nstring) const
{
    const std::u8string u8string = std::u8string((const char8_t*)nstring.data());

    const Path path = Path(u8string);
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
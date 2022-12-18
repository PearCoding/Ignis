#include "LoaderTexture.h"
#include "LoaderContext.h"
#include "Logger.h"
#include "ShadingTree.h"

#include "pattern/BrickPattern.h"
#include "pattern/CheckerBoardPattern.h"
#include "pattern/ExprPattern.h"
#include "pattern/ImagePattern.h"
#include "pattern/InvalidPattern.h"
#include "pattern/NoisePattern.h"
#include "pattern/TransformPattern.h"

namespace IG {
static std::shared_ptr<Pattern> tex_image(const std::string& name, const std::shared_ptr<SceneObject>& tex)
{
    return std::make_shared<ImagePattern>(name, tex);
}
static std::shared_ptr<Pattern> tex_checkerboard(const std::string& name, const std::shared_ptr<SceneObject>& tex)
{
    return std::make_shared<CheckerBoardPattern>(name, tex);
}
static std::shared_ptr<Pattern> tex_brick(const std::string& name, const std::shared_ptr<SceneObject>& tex)
{
    return std::make_shared<BrickPattern>(name, tex);
}
static std::shared_ptr<Pattern> tex_noise(const std::string& name, const std::shared_ptr<SceneObject>& tex)
{
    return std::make_shared<NoisePattern>(NoisePattern::Type::Noise, name, tex);
}
static std::shared_ptr<Pattern> tex_cellnoise(const std::string& name, const std::shared_ptr<SceneObject>& tex)
{
    return std::make_shared<NoisePattern>(NoisePattern::Type::CellNoise, name, tex);
}
static std::shared_ptr<Pattern> tex_pnoise(const std::string& name, const std::shared_ptr<SceneObject>& tex)
{
    return std::make_shared<NoisePattern>(NoisePattern::Type::PNoise, name, tex);
}
static std::shared_ptr<Pattern> tex_perlin(const std::string& name, const std::shared_ptr<SceneObject>& tex)
{
    return std::make_shared<NoisePattern>(NoisePattern::Type::Perlin, name, tex);
}
static std::shared_ptr<Pattern> tex_voronoi(const std::string& name, const std::shared_ptr<SceneObject>& tex)
{
    return std::make_shared<NoisePattern>(NoisePattern::Type::Voronoi, name, tex);
}
static std::shared_ptr<Pattern> tex_fbm(const std::string& name, const std::shared_ptr<SceneObject>& tex)
{
    return std::make_shared<NoisePattern>(NoisePattern::Type::FBM, name, tex);
}
static std::shared_ptr<Pattern> tex_expr(const std::string& name, const std::shared_ptr<SceneObject>& tex)
{
    return std::make_shared<ExprPattern>(name, tex);
}
static std::shared_ptr<Pattern> tex_transform(const std::string& name, const std::shared_ptr<SceneObject>& tex)
{
    return std::make_shared<TransformPattern>(name, tex);
}

using TextureLoader = std::shared_ptr<Pattern> (*)(const std::string& name, const std::shared_ptr<SceneObject>& tex);
static const struct {
    const char* Name;
    TextureLoader Loader;
} _generators[] = {
    { "image", tex_image },
    { "bitmap", tex_image },
    { "brick", tex_brick },
    { "checkerboard", tex_checkerboard },
    { "noise", tex_noise },
    { "cellnoise", tex_cellnoise },
    { "pnoise", tex_pnoise },
    { "perlin", tex_perlin },
    { "voronoi", tex_voronoi },
    { "fbm", tex_fbm },
    { "expr", tex_expr },
    { "transform", tex_transform },
    { "", nullptr }
};

void LoaderTexture::prepare(const LoaderContext& ctx)
{
    for (const auto& pair : ctx.Options.Scene->textures()) {
        const std::string name = pair.first;
        const auto obj         = pair.second;
        bool found             = false;
        for (size_t i = 0; _generators[i].Loader; ++i) {
            if (_generators[i].Name == obj->pluginType()) {
                mAvailablePatterns.emplace(name, _generators[i].Loader(name, obj));
                found = true;
                break;
            }
        }

        if (!found) {
            IG_LOG(L_ERROR) << "No texture type '" << obj->pluginType() << "' for '" << name << "' available" << std::endl;
            mAvailablePatterns.emplace(name, std::make_shared<InvalidPattern>(name));
        }
    }
}

std::string LoaderTexture::generate(const std::string& name, ShadingTree& tree)
{
    std::stringstream stream;
    auto it = mAvailablePatterns.find(name);
    if (it == mAvailablePatterns.end()) {
        IG_LOG(L_ERROR) << "Unknown texture '" << name << "'" << std::endl;
        stream << InvalidPattern::inlineError(tree.getClosureID(name)) << std::endl;
        return stream.str();
    }

    it->second->serialize(Pattern::SerializationInput{ stream, tree });

    return stream.str();
}

} // namespace IG

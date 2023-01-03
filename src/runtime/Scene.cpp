#include "Scene.h"

namespace IG {
void Scene::addFrom(const Scene& other)
{
    for (const auto& tex : other.textures())
        addTexture(tex.first, tex.second);
    for (const auto& bsdf : other.bsdfs())
        addBSDF(bsdf.first, bsdf.second);
    for (const auto& light : other.lights())
        addLight(light.first, light.second);
    for (const auto& medium : other.media())
        addLight(medium.first, medium.second);
    for (const auto& shape : other.shapes())
        addShape(shape.first, shape.second);
    for (const auto& ent : other.entities())
        addEntity(ent.first, ent.second);

    mTechnique = other.mTechnique;
    mCamera    = other.mCamera;
    mFilm      = other.mFilm;
}

void Scene::addConstantEnvLight()
{
    if (mLights.count("__env") == 0) {
        auto env = std::make_shared<SceneObject>(SceneObject::OT_LIGHT, "constant", std::filesystem::path{});
        env->setProperty("radiance", SceneProperty::fromNumber(1));
        addLight("__env", env);
    }
}
} // namespace IG
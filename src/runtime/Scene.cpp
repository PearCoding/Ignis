#include "Scene.h"
#include "Logger.h"

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
    for (const auto& param : other.parameters())
        addParameter(param.first, param.second);

    if (other.mTechnique)
        mTechnique = other.mTechnique;
    if (other.mCamera)
        mCamera = other.mCamera;
    if (other.mFilm)
        mFilm = other.mFilm;
}

void Scene::addConstantEnvLight()
{
    if (mLights.count("__env") == 0) {
        auto env = std::make_shared<SceneObject>(SceneObject::OT_LIGHT, "constant", Path{});
        env->setProperty("radiance", SceneProperty::fromNumber(1));
        addLight("__env", env);
    }
}

static void warnUnused(const std::string& prefix, const SceneObject& obj)
{
    size_t unusedCount = 0;
    for (const auto& pair : obj.properties()) {
        if (!pair.second.isUsed())
            unusedCount++;
    }

    if (unusedCount == 0)
        return;

    auto& stream = IG_LOG(L_WARNING);
    stream << prefix << " has ";
    if (unusedCount == 1)
        stream << "one unused property: [";
    else
        stream << unusedCount << " unused properties: [";

    for (const auto& pair : obj.properties()) {
        if (!pair.second.isUsed()) {
            stream << pair.first;
            if (--unusedCount > 0)
                stream << ", ";
        }
    }
    stream << "]" << std::endl;
}

void Scene::warnUnusedProperties() const
{
    if (mTechnique)
        warnUnused("Technique", *mTechnique);
    if (mCamera)
        warnUnused("Camera", *mCamera);
    if (mFilm)
        warnUnused("Film", *mFilm);

    for (const auto& tex : mTextures)
        warnUnused("Texture \"" + tex.first + "\"", *tex.second);
    for (const auto& bsdf : mBSDFs)
        warnUnused("BSDF \"" + bsdf.first + "\"", *bsdf.second);
    for (const auto& light : mLights)
        warnUnused("Light \"" + light.first + "\"", *light.second);
    for (const auto& medium : mMedia)
        warnUnused("Medium \"" + medium.first + "\"", *medium.second);
    for (const auto& shape : mShapes)
        warnUnused("Shape \"" + shape.first + "\"", *shape.second);
    for (const auto& ent : mEntities)
        warnUnused("Entity \"" + ent.first + "\"", *ent.second);
}
} // namespace IG
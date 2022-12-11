#pragma once

#include "SceneObject.h"

namespace IG {
class IG_LIB Scene {
public:
    inline Scene()
    {
    }

    Scene(const Scene& other) = default;
    Scene(Scene&& other)      = default;

    Scene& operator=(const Scene& other) = default;
    Scene& operator=(Scene&& other)      = default;

    inline std::shared_ptr<SceneObject> technique() const { return mTechnique; }
    inline void setTechnique(const std::shared_ptr<SceneObject>& technique) { mTechnique = technique; }

    inline std::shared_ptr<SceneObject> camera() const { return mCamera; }
    inline void setCamera(const std::shared_ptr<SceneObject>& camera) { mCamera = camera; }

    inline std::shared_ptr<SceneObject> film() const { return mFilm; }
    inline void setFilm(const std::shared_ptr<SceneObject>& film) { mFilm = film; }

    inline const auto& textures() const { return mTextures; }
    inline const auto& bsdfs() const { return mBSDFs; }
    inline const auto& shapes() const { return mShapes; }
    inline const auto& lights() const { return mLights; }
    inline const auto& media() const { return mMedia; }
    inline const auto& entities() const { return mEntities; }

    inline std::shared_ptr<SceneObject> texture(const std::string& name) const { return mTextures.count(name) > 0 ? mTextures.at(name) : nullptr; }
    inline std::shared_ptr<SceneObject> bsdf(const std::string& name) const { return mBSDFs.count(name) > 0 ? mBSDFs.at(name) : nullptr; }
    inline std::shared_ptr<SceneObject> shape(const std::string& name) const { return mShapes.count(name) > 0 ? mShapes.at(name) : nullptr; }
    inline std::shared_ptr<SceneObject> light(const std::string& name) const { return mLights.count(name) > 0 ? mLights.at(name) : nullptr; }
    inline std::shared_ptr<SceneObject> medium(const std::string& name) const { return mMedia.count(name) > 0 ? mMedia.at(name) : nullptr; }
    inline std::shared_ptr<SceneObject> entity(const std::string& name) const { return mEntities.count(name) > 0 ? mEntities.at(name) : nullptr; }

    inline void addTexture(const std::string& name, const std::shared_ptr<SceneObject>& texture) { mTextures[name] = texture; }
    inline void addBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf) { mBSDFs[name] = bsdf; }
    inline void addShape(const std::string& name, const std::shared_ptr<SceneObject>& shape) { mShapes[name] = shape; }
    inline void addLight(const std::string& name, const std::shared_ptr<SceneObject>& light) { mLights[name] = light; }
    inline void addMedium(const std::string& name, const std::shared_ptr<SceneObject>& medium) { mMedia[name] = medium; }
    inline void addEntity(const std::string& name, const std::shared_ptr<SceneObject>& entity) { mEntities[name] = entity; }

    /// Add all information from other to this scene, replacing present information
    void addFrom(const Scene& other);

    void addConstantEnvLight();

private:
    std::shared_ptr<SceneObject> mTechnique;
    std::shared_ptr<SceneObject> mCamera;
    std::shared_ptr<SceneObject> mFilm;

    std::unordered_map<std::string, std::shared_ptr<SceneObject>> mTextures;
    std::unordered_map<std::string, std::shared_ptr<SceneObject>> mBSDFs;
    std::unordered_map<std::string, std::shared_ptr<SceneObject>> mShapes;
    std::unordered_map<std::string, std::shared_ptr<SceneObject>> mLights;
    std::unordered_map<std::string, std::shared_ptr<SceneObject>> mMedia;
    std::unordered_map<std::string, std::shared_ptr<SceneObject>> mEntities;
};
} // namespace IG
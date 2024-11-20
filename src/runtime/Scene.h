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
    inline const auto& parameters() const { return mParameters; }

    inline std::shared_ptr<SceneObject> texture(const std::string& name) const { return get(mTextures, name); }
    inline std::shared_ptr<SceneObject> bsdf(const std::string& name) const { return get(mBSDFs, name); }
    inline std::shared_ptr<SceneObject> shape(const std::string& name) const { return get(mShapes, name); }
    inline std::shared_ptr<SceneObject> light(const std::string& name) const { return get(mLights, name); }
    inline std::shared_ptr<SceneObject> medium(const std::string& name) const { return get(mMedia, name); }
    inline std::shared_ptr<SceneObject> entity(const std::string& name) const { return get(mEntities, name); }
    inline std::shared_ptr<SceneObject> parameter(const std::string& name) const { return get(mParameters, name); }

    inline void addTexture(const std::string& name, const std::shared_ptr<SceneObject>& texture) { mTextures[name] = texture; }
    inline void addBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf) { mBSDFs[name] = bsdf; }
    inline void addShape(const std::string& name, const std::shared_ptr<SceneObject>& shape) { mShapes[name] = shape; }
    inline void addLight(const std::string& name, const std::shared_ptr<SceneObject>& light) { mLights[name] = light; }
    inline void addMedium(const std::string& name, const std::shared_ptr<SceneObject>& medium) { mMedia[name] = medium; }
    inline void addEntity(const std::string& name, const std::shared_ptr<SceneObject>& entity) { mEntities[name] = entity; }
    inline void addParameter(const std::string& name, const std::shared_ptr<SceneObject>& param) { mParameters[name] = param; }

    /// Add all information from other to this scene, replacing present information
    void addFrom(const Scene& other);

    void addConstantEnvLight();

    void warnUnusedProperties() const;

private:
    using ObjectMap = std::unordered_map<std::string, std::shared_ptr<SceneObject>>;

    inline std::shared_ptr<SceneObject> get(const ObjectMap& map, const std::string& name) const
    {
        if (auto it = map.find(name); it != map.end())
            return it->second;
        else
            return nullptr;
    }

    std::shared_ptr<SceneObject> mTechnique;
    std::shared_ptr<SceneObject> mCamera;
    std::shared_ptr<SceneObject> mFilm;

    ObjectMap mTextures;
    ObjectMap mBSDFs;
    ObjectMap mLights;
    ObjectMap mMedia;
    ObjectMap mShapes;
    ObjectMap mEntities;
    ObjectMap mParameters;
};
} // namespace IG
#pragma once

#include "IG_Config.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace IG::Parser {
// --------------- Enums
enum ObjectType {
    OT_BSDF = 0,
    OT_CAMERA,
    OT_ENTITY,
    OT_FILM,
    OT_LIGHT,
    OT_MEDIUM,
    OT_SHAPE,
    OT_TECHNIQUE,
    OT_TEXTURE,
    _OT_COUNT
};

enum PropertyType {
    PT_NONE = 0,
    PT_BOOL,
    PT_INTEGER,
    PT_NUMBER,
    PT_STRING,
    PT_TRANSFORM,
    PT_VECTOR2,
    PT_VECTOR3
};

using Number  = float;
using Integer = int32;

// --------------- Property
class Property {
public:
    inline Property()
        : mType(PT_NONE)
    {
    }

    Property(const Property& other) = default;
    Property(Property&& other)      = default;

    Property& operator=(const Property& other) = default;
    Property& operator=(Property&& other)      = default;

    inline PropertyType type() const { return mType; }
    inline bool isValid() const { return mType != PT_NONE; }
    inline bool canBeNumber() const { return mType == PT_NUMBER || mType == PT_INTEGER; }

    inline Number getNumber(Number def = Number(0), bool* ok = nullptr, bool exact = false) const
    {
        if (mType == PT_NUMBER) {
            if (ok)
                *ok = true;
            return mNumber;
        } else if (!exact && mType == PT_INTEGER) {
            if (ok)
                *ok = true;
            return static_cast<Number>(mInteger);
        } else {
            if (ok)
                *ok = false;
            return def;
        }
    }

    static inline Property fromNumber(Number v)
    {
        Property p(PT_NUMBER);
        p.mNumber = v;
        return p;
    }

    inline Integer getInteger(Integer def = Integer(0), bool* ok = nullptr) const
    {
        if (mType == PT_INTEGER) {
            if (ok)
                *ok = true;
            return mInteger;
        } else {
            if (ok)
                *ok = false;
            return def;
        }
    }

    static inline Property fromInteger(Integer v)
    {
        Property p(PT_INTEGER);
        p.mInteger = v;
        return p;
    }

    inline bool getBool(bool def = false, bool* ok = nullptr) const
    {
        if (mType == PT_BOOL) {
            if (ok)
                *ok = true;
            return mBool;
        } else {
            if (ok)
                *ok = false;
            return def;
        }
    }

    static inline Property fromBool(bool b)
    {
        Property p(PT_BOOL);
        p.mBool = b;
        return p;
    }

    inline const Vector2f& getVector2(const Vector2f& def = Vector2f(0, 0), bool* ok = nullptr) const
    {
        if (mType == PT_VECTOR2) {
            if (ok)
                *ok = true;
            return mVector2;
        } else {
            if (ok)
                *ok = false;
            return def;
        }
    }

    static inline Property fromVector2(const Vector2f& v)
    {
        Property p(PT_VECTOR2);
        p.mVector2 = v;
        return p;
    }

    inline const Vector3f& getVector3(const Vector3f& def = Vector3f(0, 0, 0), bool* ok = nullptr) const
    {
        if (mType == PT_VECTOR3) {
            if (ok)
                *ok = true;
            return mVector3;
        } else {
            if (ok)
                *ok = false;
            return def;
        }
    }

    static inline Property fromVector3(const Vector3f& v)
    {
        Property p(PT_VECTOR3);
        p.mVector3 = v;
        return p;
    }

    inline const Transformf& getTransform(const Transformf& def = Transformf::Identity(), bool* ok = nullptr) const
    {
        if (mType == PT_TRANSFORM) {
            if (ok)
                *ok = true;
            return mTransform;
        } else {
            if (ok)
                *ok = false;
            return def;
        }
    }

    static inline Property fromTransform(const Transformf& v)
    {
        Property p(PT_TRANSFORM);
        p.mTransform = v;
        return p;
    }

    inline const std::string& getString(const std::string& def = "", bool* ok = nullptr) const
    {
        if (mType == PT_STRING) {
            if (ok)
                *ok = true;
            return mString;
        } else {
            if (ok)
                *ok = false;
            return def;
        }
    }

    static inline Property fromString(const std::string& v)
    {
        Property p(PT_STRING);
        p.mString = v;
        return p;
    }

private:
    inline explicit Property(PropertyType type)
        : mType(type)
    {
    }

    PropertyType mType;

    // Data Types
    union {
        Number mNumber;
        Integer mInteger;
        bool mBool;
    };
    std::string mString;
    Vector3f mVector3;
    Vector2f mVector2;
    Transformf mTransform;
};

// --------------- Object
class Object {
public:
    inline explicit Object(ObjectType type, const std::string& pluginType, const std::filesystem::path& baseDir)
        : mType(type)
        , mPluginType(pluginType)
        , mBaseDir(baseDir)
    {
    }

    Object(const Object& other) = default;
    Object(Object&& other)      = default;

    Object& operator=(const Object& other) = default;
    Object& operator=(Object&& other)      = default;

    inline ObjectType type() const { return mType; }
    inline const std::string& pluginType() const { return mPluginType; }
    inline const std::filesystem::path& baseDir() const { return mBaseDir; }

    inline Property property(const std::string& key) const
    {
        return mProperties.count(key) ? mProperties.at(key) : Property();
    }

    inline void setProperty(const std::string& key, const Property& prop) { mProperties[key] = prop; }
    inline const std::unordered_map<std::string, Property>& properties() const { return mProperties; }

    inline Property& operator[](const std::string& key) { return mProperties[key]; }
    inline Property operator[](const std::string& key) const { return property(key); }

private:
    ObjectType mType;
    std::string mPluginType;
    std::filesystem::path mBaseDir;
    std::unordered_map<std::string, Property> mProperties;
};

// --------------- Scene
class Scene {
public:
    inline Scene()
    {
    }

    Scene(const Scene& other) = default;
    Scene(Scene&& other)      = default;

    Scene& operator=(const Scene& other) = default;
    Scene& operator=(Scene&& other)      = default;

    inline std::shared_ptr<Object> technique() const { return mTechnique; }
    inline void setTechnique(const std::shared_ptr<Object>& technique) { mTechnique = technique; }

    inline std::shared_ptr<Object> camera() const { return mCamera; }
    inline void setCamera(const std::shared_ptr<Object>& camera) { mCamera = camera; }

    inline std::shared_ptr<Object> film() const { return mFilm; }
    inline void setFilm(const std::shared_ptr<Object>& film) { mFilm = film; }

    inline const auto& textures() const { return mTextures; }
    inline const auto& bsdfs() const { return mBSDFs; }
    inline const auto& shapes() const { return mShapes; }
    inline const auto& lights() const { return mLights; }
    inline const auto& media() const { return mMedia; }
    inline const auto& entities() const { return mEntities; }

    inline std::shared_ptr<Object> texture(const std::string& name) const { return mTextures.count(name) > 0 ? mTextures.at(name) : nullptr; }
    inline std::shared_ptr<Object> bsdf(const std::string& name) const { return mBSDFs.count(name) > 0 ? mBSDFs.at(name) : nullptr; }
    inline std::shared_ptr<Object> shape(const std::string& name) const { return mShapes.count(name) > 0 ? mShapes.at(name) : nullptr; }
    inline std::shared_ptr<Object> light(const std::string& name) const { return mLights.count(name) > 0 ? mLights.at(name) : nullptr; }
    inline std::shared_ptr<Object> medium(const std::string& name) const { return mMedia.count(name) > 0 ? mMedia.at(name) : nullptr; }
    inline std::shared_ptr<Object> entity(const std::string& name) const { return mEntities.count(name) > 0 ? mEntities.at(name) : nullptr; }

    inline void addTexture(const std::string& name, const std::shared_ptr<Object>& texture) { mTextures[name] = texture; }
    inline void addBSDF(const std::string& name, const std::shared_ptr<Object>& bsdf) { mBSDFs[name] = bsdf; }
    inline void addShape(const std::string& name, const std::shared_ptr<Object>& shape) { mShapes[name] = shape; }
    inline void addLight(const std::string& name, const std::shared_ptr<Object>& light) { mLights[name] = light; }
    inline void addMedium(const std::string& name, const std::shared_ptr<Object>& medium) { mMedia[name] = medium; }
    inline void addEntity(const std::string& name, const std::shared_ptr<Object>& entity) { mEntities[name] = entity; }

    // Add all information from other to this scene, except technique, film and camera information
    void addFrom(const Scene& other);

    void addConstantEnvLight();

private:
    std::shared_ptr<Object> mTechnique;
    std::shared_ptr<Object> mCamera;
    std::shared_ptr<Object> mFilm;

    std::unordered_map<std::string, std::shared_ptr<Object>> mTextures;
    std::unordered_map<std::string, std::shared_ptr<Object>> mBSDFs;
    std::unordered_map<std::string, std::shared_ptr<Object>> mShapes;
    std::unordered_map<std::string, std::shared_ptr<Object>> mLights;
    std::unordered_map<std::string, std::shared_ptr<Object>> mMedia;
    std::unordered_map<std::string, std::shared_ptr<Object>> mEntities;
};

// --------------- SceneParser
class SceneParser {
    friend class InternalSceneParser;

public:
    inline SceneParser() = default;

    /// @brief Load from file, lookup directory will be the directory the file is in
    /// @param path Path to file containing scene description
    /// @param ok True if everything is fine
    /// @return Scene, only valid if ok equals true
    Scene loadFromFile(const std::filesystem::path& path, bool& ok);

    /// @brief Load from string, lookup directory can be set optionally, else external files depend on the current working directory
    /// @param str String containing scene description
    /// @param opt_dir Optional directory containing external files if not given as absolute files inside the scene description
    /// @param ok True if everything is fine
    /// @return Scene, only valid if ok equals true
    Scene loadFromString(const std::string& str, const std::filesystem::path& opt_dir, bool& ok);

    inline void addArgument(const std::string& key, const std::string& value)
    {
        mArguments[key] = value;
    }

private:
    std::unordered_map<std::string, std::string> mArguments;
};
} // namespace IG::Parser
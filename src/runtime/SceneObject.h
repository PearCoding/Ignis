#pragma once

#include "SceneProperty.h"
#include <memory>
#include <unordered_map>

namespace IG {
class SceneObject {
public:
    enum Type {
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

    inline explicit SceneObject(Type type, const std::string& pluginType, const std::filesystem::path& baseDir)
        : mType(type)
        , mPluginType(pluginType)
        , mBaseDir(baseDir)
    {
    }

    SceneObject(const SceneObject& other) = default;
    SceneObject(SceneObject&& other)      = default;

    SceneObject& operator=(const SceneObject& other) = default;
    SceneObject& operator=(SceneObject&& other)      = default;

    inline Type type() const { return mType; }
    inline const std::string& pluginType() const { return mPluginType; }
    inline const std::filesystem::path& baseDir() const { return mBaseDir; }

    inline SceneProperty property(const std::string& key) const
    {
        if (auto it = mProperties.find(key); it != mProperties.end())
            return it->second;
        else
            return SceneProperty();
    }

    /// @brief Guarded property access for heavy weight properties, due to the reference access. No copy is made
    inline std::optional<std::reference_wrapper<const SceneProperty>> propertyOpt(const std::string& key) const
    {
        if (auto it = mProperties.find(key); it != mProperties.end())
            return std::cref(it->second);
        else
            return std::nullopt;
    }

    inline void setProperty(const std::string& key, const SceneProperty& prop) { mProperties[key] = prop; }
    inline const std::unordered_map<std::string, SceneProperty>& properties() const { return mProperties; }

    inline SceneProperty& operator[](const std::string& key) { return mProperties[key]; }
    inline SceneProperty operator[](const std::string& key) const { return property(key); }

private:
    Type mType;
    std::string mPluginType;
    std::filesystem::path mBaseDir;
    std::unordered_map<std::string, SceneProperty> mProperties;
};

} // namespace IG
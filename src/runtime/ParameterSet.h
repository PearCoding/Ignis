#pragma once

#include "IG_Config.h"

namespace IG {
struct IG_LIB ParameterSet {
    std::unordered_map<std::string, int> IntParameters;
    std::unordered_map<std::string, float> FloatParameters;
    AlignedUnorderedMap<std::string, Vector3f> VectorParameters;
    AlignedUnorderedMap<std::string, Vector4f> ColorParameters;

    inline bool empty() const { return IntParameters.empty() && FloatParameters.empty() && VectorParameters.empty() && ColorParameters.empty(); }

    /// @brief Dump current parameter set information as a multi-line string for debug purposes
    std::string dump() const;

    /// @brief Will merge `other` into this. If replace true, will replace regardless if already defined
    void mergeFrom(const ParameterSet& other, bool replace = false);

    inline void set(const std::string& key, int value)
    {
        IntParameters[key] = value;
    }

    inline void set(const std::string& key, float value)
    {
        FloatParameters[key] = value;
    }

    inline void set(const std::string& key, const Vector3f& value)
    {
        VectorParameters[key] = value;
    }

    inline void set(const std::string& key, const Vector4f& value)
    {
        ColorParameters[key] = value;
    }

    inline int getInt(const std::string& key, int def = 0) const
    {
        if (const auto it = IntParameters.find(key); it != IntParameters.end())
            return it->second;
        else
            return def;
    }

    inline float getFloat(const std::string& key, float def = 0.0f) const
    {
        if (const auto it = FloatParameters.find(key); it != FloatParameters.end())
            return it->second;
        else
            return def;
    }

    inline Vector3f getVector(const std::string& key, const Vector3f& def = Vector3f::Zero()) const
    {
        if (const auto it = VectorParameters.find(key); it != VectorParameters.end())
            return it->second;
        else
            return def;
    }

    inline Vector4f getColor(const std::string& key, const Vector4f& def = Vector4f::Zero()) const
    {
        if (const auto it = ColorParameters.find(key); it != ColorParameters.end())
            return it->second;
        else
            return def;
    }
};
} // namespace IG
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
};
} // namespace IG
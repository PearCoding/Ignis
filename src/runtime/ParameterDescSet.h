#pragma once

#include "IG_Config.h"

namespace IG {
/// @brief Basic set of descriptions for parameters. In contrary to ParameterSet, this has no runtime values and is to a large extend optional.
class IG_LIB ParameterDescSet {
public:
    struct IntParameter {
        std::string Display;
        std::string Description;
        int Default;
        int Min;
        int Max;
        int Step;
        bool Internal;
    };

    struct FloatParameter {
        std::string Display;
        std::string Description;
        float Default;
        float Min;
        float Max;
        float Step;
        bool Internal;
    };

    struct VectorParameter {
        std::string Display;
        std::string Description;
        Vector3f Default;
        bool Normalized;
        bool Internal;
    };

    struct ColorParameter {
        std::string Display;
        std::string Description;
        Vector4f Default;
        bool Internal;
    };

    struct StringParameter {
        std::string Display;
        std::string Description;
        std::string Default;
        bool Internal;
    };

    inline void add(const std::string& key, const IntParameter& info)
    {
        mIntParameters[key] = info;
    }

    inline void add(const std::string& key, const FloatParameter& info)
    {
        mFloatParameters[key] = info;
    }

    inline void add(const std::string& key, const VectorParameter& info)
    {
        mVectorParameters[key] = info;
    }

    inline void add(const std::string& key, const ColorParameter& info)
    {
        mColorParameters[key] = info;
    }

    inline void add(const std::string& key, const StringParameter& info)
    {
        mStringParameters[key] = info;
    }

    inline std::optional<IntParameter> getInt(const std::string& key) const
    {
        if (const auto it = mIntParameters.find(key); it != mIntParameters.end())
            return it->second;
        else
            return {};
    }

    inline std::optional<FloatParameter> getFloat(const std::string& key) const
    {
        if (const auto it = mFloatParameters.find(key); it != mFloatParameters.end())
            return it->second;
        else
            return {};
    }

    inline std::optional<VectorParameter> getVector(const std::string& key) const
    {
        if (const auto it = mVectorParameters.find(key); it != mVectorParameters.end())
            return it->second;
        else
            return {};
    }

    inline std::optional<ColorParameter> getColor(const std::string& key) const
    {
        if (const auto it = mColorParameters.find(key); it != mColorParameters.end())
            return it->second;
        else
            return {};
    }

    inline std::optional<StringParameter> getString(const std::string& key) const
    {
        if (const auto it = mStringParameters.find(key); it != mStringParameters.end())
            return it->second;
        else
            return {};
    }

    [[nodiscard]] inline const auto& intParameters() const { return mIntParameters; }
    [[nodiscard]] inline const auto& floatParameters() const { return mFloatParameters; }
    [[nodiscard]] inline const auto& vectorParameters() const { return mVectorParameters; }
    [[nodiscard]] inline const auto& colorParameters() const { return mColorParameters; }
    [[nodiscard]] inline const auto& stringParameters() const { return mStringParameters; }

    [[nodiscard]] inline bool empty(bool includeInternal = true) const
    {
        return numberOfParameters(includeInternal) == 0;
    }

    [[nodiscard]] inline size_t numberOfParameters(bool includeInternal = true) const
    {
        return numberOfIntParameters(includeInternal) + numberOfFloatParameters(includeInternal) + numberOfVectorParameters(includeInternal) + numberOfColorParameters(includeInternal) + numberOfStringParameters(includeInternal);
    }

    [[nodiscard]] inline size_t numberOfIntParameters(bool includeInternal = true) const
    {
        if (includeInternal)
            return mIntParameters.size();
        else
            return std::count_if(mIntParameters.begin(), mIntParameters.end(), [](auto p) { return !p.second.Internal; });
    }

    [[nodiscard]] inline size_t numberOfFloatParameters(bool includeInternal = true) const
    {
        if (includeInternal)
            return mFloatParameters.size();
        else
            return std::count_if(mFloatParameters.begin(), mFloatParameters.end(), [](auto p) { return !p.second.Internal; });
    }

    [[nodiscard]] inline size_t numberOfVectorParameters(bool includeInternal = true) const
    {
        if (includeInternal)
            return mVectorParameters.size();
        else
            return std::count_if(mVectorParameters.begin(), mVectorParameters.end(), [](auto p) { return !p.second.Internal; });
    }

    [[nodiscard]] inline size_t numberOfColorParameters(bool includeInternal = true) const
    {
        if (includeInternal)
            return mColorParameters.size();
        else
            return std::count_if(mColorParameters.begin(), mColorParameters.end(), [](auto p) { return !p.second.Internal; });
    }

    [[nodiscard]] inline size_t numberOfStringParameters(bool includeInternal = true) const
    {
        if (includeInternal)
            return mStringParameters.size();
        else
            return std::count_if(mStringParameters.begin(), mStringParameters.end(), [](auto p) { return !p.second.Internal; });
    }

private:
    std::unordered_map<std::string, IntParameter> mIntParameters;
    std::unordered_map<std::string, FloatParameter> mFloatParameters;
    std::unordered_map<std::string, VectorParameter> mVectorParameters;
    std::unordered_map<std::string, ColorParameter> mColorParameters;
    std::unordered_map<std::string, StringParameter> mStringParameters;
};

} // namespace IG
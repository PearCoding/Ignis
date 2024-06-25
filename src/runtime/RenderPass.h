#pragma once

#include "ParameterSet.h"

namespace IG {
class Runtime;

class IG_LIB RenderPass {
public:
    RenderPass(Runtime* runtime, void* callback);
    ~RenderPass();

    /// Set integer parameter in the registry. Will replace already present values
    inline void setParameter(const std::string& name, int value)
    {
        mRegistry->set(name, value);
    }

    /// Set number parameter in the registry. Will replace already present values
    inline void setParameter(const std::string& name, float value)
    {
        mRegistry->set(name, value);
    }

    /// Set 3d vector parameter in the registry. Will replace already present values
    inline void setParameter(const std::string& name, const Vector3f& value)
    {
        mRegistry->set(name, value);
    }

    /// Set 4d vector parameter in the registry. Will replace already present values
    inline void setParameter(const std::string& name, const Vector4f& value)
    {
        mRegistry->set(name, value);
    }

    bool run();

    [[nodiscard]] size_t getOutputSizeInBytes(const std::string& name) const;
    bool copyOutputToHost(const std::string& name, void* dst, size_t maxSizeInBytes);

    [[nodiscard]] inline std::shared_ptr<ParameterSet> parameter() const { return mRegistry; }
    [[nodiscard]] inline void* internalCallback() const { return mCallback; }

private:
    Runtime* const mRuntime;
    void* const mCallback;

    std::shared_ptr<ParameterSet> mRegistry;
};
} // namespace IG
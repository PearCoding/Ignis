#pragma once

#include "StatisticTypes.h"

namespace IG {

// Connects type and sub id to a unique id
class SmallShaderKey {
public:
    SmallShaderKey(ShaderType type, uint32 subId = 0)
        : mType(type)
        , mSubID(subId)
    {
    }

    inline size_t id() const
    {
        static_assert(sizeof(size_t) >= sizeof(uint32), "Expected a 64bit machine");
        return (static_cast<size_t>(mType) << 32) | static_cast<size_t>(mSubID);
    }

    inline ShaderType type() const { return mType; }
    inline uint32 subID() const { return mSubID; }

private:
    ShaderType mType;
    uint32 mSubID;
};

inline bool operator==(const SmallShaderKey& a, const SmallShaderKey& b)
{
    return a.id() == b.id();
}

struct SmallShaderKeyHash {
    inline size_t operator()(const SmallShaderKey& val) const
    {
        return val.id();
    }
};
} // namespace IG
#pragma once

#include "Statistics.h"

namespace IG {

// Connects variant, type and sub id to a unique id
class ShaderKey {
public:
    ShaderKey(ShaderType type, uint32 subId)
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

inline bool operator==(const ShaderKey& a, const ShaderKey& b)
{
    return a.id() == b.id();
}

struct ShaderKeyHash {
    inline size_t operator()(const ShaderKey& val) const
    {
        return val.id();
    }
};
} // namespace IG
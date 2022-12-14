#pragma once

#include "Statistics.h"

namespace IG {

// Connects variant, type and sub id to a unique id
class ShaderKey {
public:
    ShaderKey(uint32 variant, ShaderType type, uint32 subId)
        : mVariant(variant)
        , mType(type)
        , mSubID(subId)
    {
    }

    inline size_t id() const
    {
        static_assert(sizeof(size_t) >= sizeof(uint32), "Expected a 64bit machine");
        return static_cast<size_t>(mVariant) << 32 | static_cast<size_t>(mType) << 16 << static_cast<size_t>(mSubID);
    }

    inline uint32 variant() const { return mVariant; }
    inline ShaderType type() const { return mType; }
    inline uint32 subID() const { return mSubID; }

private:
    uint32 mVariant;
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
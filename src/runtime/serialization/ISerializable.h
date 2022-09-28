#pragma once

#include "IG_Config.h"

namespace IG {
class Serializer;
class ISerializable {
public:
    virtual void serialize(Serializer& serializer) = 0;
};

} // namespace IG

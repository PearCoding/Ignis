#pragma once

#include "IG_Config.h"

namespace IG {
struct TriShape {
    size_t VertexCount;
    size_t NormalCount;
    size_t TexCount;
    size_t FaceCount;
    float Area;
    size_t BvhID;
};
}
#pragma once

#include "IG_Config.h"

namespace IG {

enum class ShaderType {
    Device = 0,
    PrimaryTraversal,
    SecondaryTraversal,
    RayGeneration,
    Hit,
    Miss,
    AdvancedShadowHit,
    AdvancedShadowMiss,
    Callback,
    Tonemap,
    Glare,
    ImageInfo,
    Bake,

    _COUNT
};

enum class SectionType {
    // This should be in sync with core/stats.art
    GPUSortPrimary         = 0,
    GPUSortSecondary       = 1,
    GPUCompactPrimary      = 2,
    GPUSortPrimaryReset    = 3,
    GPUSortPrimaryCount    = 4,
    GPUSortPrimaryScan     = 5,
    GPUSortPrimarySort     = 6,
    GPUSortPrimaryCollapse = 7,
    // TODO: Add for CPU as well (vectorization kinda gets in the way though)
    ImageInfoPercentile = 10,
    ImageInfoError      = 11,
    ImageInfoHistogram  = 12,

    ImageLoading,
    PackedImageLoading,
    BufferLoading,
    BufferRequests,
    BufferReleases,
    FramebufferUpdate,
    AOVUpdate,
    TonemapUpdate,
    FramebufferHostUpdate,
    AOVHostUpdate,

    _COUNT
};

enum class Quantity {
    // This should be in sync with core/stats.art
    CameraRayCount = 0,
    ShadowRayCount,
    BounceRayCount,

    _COUNT
};
}
#pragma once

#include "IG_Config.h"

namespace IG {
struct TonemapSettings {
    const char* AOV;
    size_t Method;
    bool UseGamma;
    float Scale;
    float ExposureFactor;
    float ExposureOffset;
};

struct GlareSettings {
    const char* AOV;
    float LuminanceAverage;
    float LuminanceMultiplier;
};

struct ImageInfoSettings {
    const char* AOV;
    int* Histogram;
    size_t Bins;
    float Scale;
};

struct ImageInfoOutput {
    float Min;
    float Max;
    float Average;
    float SoftMin;
    float SoftMax;
    float Median;
};

struct ParameterSet {
    std::unordered_map<std::string, int> IntParameters;
    std::unordered_map<std::string, float> FloatParameters;
    AlignedUnorderedMap<std::string, Vector3f> VectorParameters;
    AlignedUnorderedMap<std::string, Vector4f> ColorParameters;
};

struct Ray {
    Vector3f Origin;
    Vector3f Direction;
    Vector2f Range;
};
} // namespace IG
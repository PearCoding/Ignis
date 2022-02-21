#pragma once

#include "IG_Config.h"

namespace IG {
struct TonemapSettings {
    int AOV;
    int Method;
    bool UseGamma;
    float Scale;
    float ExposureFactor;
    float ExposureOffset;
};

struct ImageInfoSettings {
    int AOV;
    int* Histogram;
    int Bins;
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
    std::unordered_map<std::string, Vector3f> VectorParameters;
    std::unordered_map<std::string, Vector4f> ColorParameters;
};
} // namespace IG
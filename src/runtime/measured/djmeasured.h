#pragma once

#include "IG_Config.h"

namespace IG {
namespace measured {

struct Warp {
    int size_x, size_y;
    float patch_size_x, patch_size_y;
    float inv_patch_size_x, inv_patch_size_y;
    unsigned int* param_size;
    unsigned int* param_strides;
    float* param_values;
    int param_values_offset;
    float* data;
    float* marginal_cdf;
    float* conditional_cdf;
    unsigned int* array_sizes;
};

struct BRDFData {
    Warp ndf, sigma, vndf, luminance, rgb;
    bool isotropic, jacobian;
};

BRDFData* load_brdf_data(const Path& path);
void write_brdf_data(BRDFData* data, const Path& path);

} // namespace measured
} // namespace IG
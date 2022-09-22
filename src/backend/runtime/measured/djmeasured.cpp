#include "djmeasured.h"
#include "Logger.h"
#include "serialization/FileSerializer.h"

#define POWITACQ_IMPLEMENTATION
#include "powitacq_rgb.h"

namespace IG {
namespace measured {
template <unsigned int Dimension>
Warp convert_warp(powitacq_rgb::Marginal2D<Dimension> in_warp)
{
    unsigned int* param_sizes = new unsigned int[Dimension];
    for (unsigned int d = 0; d < Dimension; d++) {
        param_sizes[d] = (unsigned int)in_warp.m_param_values[d].size();
    }

    unsigned int* param_strides = new unsigned int[Dimension];
    for (unsigned int d = 0; d < Dimension; d++) {
        param_strides[d] = in_warp.m_param_strides[d];
    }

    size_t param_values_offset = 0;
    for (unsigned int d = 0; d < Dimension; d++) {
        param_values_offset = std::max(param_values_offset, in_warp.m_param_values[d].size());
    }

    float* param_values = new float[param_values_offset * Dimension];
    memset(param_values, 0, sizeof(float) * param_values_offset * Dimension);
    for (size_t i = 0; i < Dimension; i++) {
        memcpy(&param_values[i * param_values_offset],
               in_warp.m_param_values[i].data(),
               sizeof(float) * in_warp.m_param_values[i].size());
    }

    float* data = new float[in_warp.m_data.size()];
    memcpy(data, in_warp.m_data.data(), in_warp.m_data.size() * sizeof(float));

    float* marginal_cdf = new float[in_warp.m_marginal_cdf.size()];
    memcpy(marginal_cdf, in_warp.m_marginal_cdf.data(), in_warp.m_marginal_cdf.size() * sizeof(float));

    float* conditional_cdf = new float[in_warp.m_conditional_cdf.size()];
    memcpy(conditional_cdf, in_warp.m_conditional_cdf.data(), in_warp.m_conditional_cdf.size() * sizeof(float));

    unsigned int* array_sizes = new unsigned int[6]{ Dimension, Dimension, (unsigned int)param_values_offset * Dimension, (unsigned int)in_warp.m_data.size(), (unsigned int)in_warp.m_marginal_cdf.size(), (unsigned int)in_warp.m_conditional_cdf.size() };

    return Warp{
        (int)in_warp.m_size.x(),
        (int)in_warp.m_size.y(),
        in_warp.m_patch_size.x(),
        in_warp.m_patch_size.y(),
        in_warp.m_inv_patch_size.x(),
        in_warp.m_inv_patch_size.y(),
        param_sizes,
        param_strides,
        param_values,
        (int)param_values_offset,
        data,
        marginal_cdf,
        conditional_cdf,
        array_sizes
    };
}

BRDFData* convert_brdf(powitacq_rgb::BRDF* brdf)
{
    auto brdf_data = brdf->m_data.get();

    return new BRDFData{
        convert_warp<0>(brdf_data->ndf),
        convert_warp<0>(brdf_data->sigma),
        convert_warp<2>(brdf_data->vndf),
        convert_warp<2>(brdf_data->luminance),
        convert_warp<3>(brdf_data->rgb),
        brdf_data->isotropic,
        brdf_data->jacobian,
    };
}

std::vector<float> linearize_warp(const Warp* warp)
{
    static_assert(sizeof(float) == sizeof(int));
    std::vector<float> data;
    data.clear();
    for (int i = 0; i < 6; i++) {
        data.push_back((float)(warp->array_sizes[i]));
    }

    data.push_back((float)(warp->size_x));
    data.push_back((float)(warp->size_y));
    data.push_back(warp->patch_size_x);
    data.push_back(warp->patch_size_y);
    data.push_back(warp->inv_patch_size_x);
    data.push_back(warp->inv_patch_size_y);
    data.push_back((float)(warp->param_values_offset));

    for (unsigned int i = 0; i < warp->array_sizes[0]; i++)
        data.push_back((float)(warp->param_size[i]));

    for (unsigned int i = 0; i < warp->array_sizes[1]; i++)
        data.push_back((float)(warp->param_strides[i]));

    for (unsigned int i = 0; i < warp->array_sizes[2]; i++)
        data.push_back(warp->param_values[i]);

    for (unsigned int i = 0; i < warp->array_sizes[3]; i++)
        data.push_back(warp->data[i]);

    for (unsigned int i = 0; i < warp->array_sizes[4]; i++)
        data.push_back(warp->marginal_cdf[i]);

    for (unsigned int i = 0; i < warp->array_sizes[5]; i++)
        data.push_back(warp->conditional_cdf[i]);

    return data;
}

BRDFData* load_brdf_data(const std::filesystem::path& brdf_path)
{
    powitacq_rgb::BRDF* brdf = new powitacq_rgb::BRDF(brdf_path.generic_u8string());
    BRDFData* data           = convert_brdf(brdf);
    return data;
}

void write_brdf_data(BRDFData* data, const std::filesystem::path& path)
{
    auto ndf       = linearize_warp(&data->ndf);
    auto vndf      = linearize_warp(&data->vndf);
    auto sigma     = linearize_warp(&data->sigma);
    auto luminance = linearize_warp(&data->luminance);
    auto rgb       = linearize_warp(&data->rgb);

    IG::FileSerializer os_ndf(path.generic_string() + "_ndf", false);
    os_ndf.write(ndf, true);
    os_ndf.close();

    IG::FileSerializer os_vndf(path.generic_string() + "_vndf", false);
    os_vndf.write(vndf, true);
    os_vndf.close();

    IG::FileSerializer os_sigma(path.generic_string() + "_sigma", false);
    os_sigma.write(sigma, true);
    os_sigma.close();

    IG::FileSerializer os_luminance(path.generic_string() + "_luminance", false);
    os_luminance.write(luminance, true);
    os_luminance.close();

    IG::FileSerializer os_rgb(path.generic_string() + "_rgb", false);
    os_rgb.write(rgb, true);
    os_rgb.close();
}

} // namespace measured
} // namespace IG
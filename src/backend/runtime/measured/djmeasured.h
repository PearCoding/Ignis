#include <fstream>
#include <vector>

struct Warp {
    int size_x, size_y;
    float patch_size_x, patch_size_y;
    float inv_patch_size_x, inv_patch_size_y;
    unsigned int * param_size;
    unsigned int * param_strides;
    float * param_values;
    int param_values_offset;
    float * data;
    float * marginal_cdf;
    float * conditional_cdf;
    unsigned int * array_sizes;
};

struct BRDFData {
    Warp ndf, sigma, vndf, luminance, rgb;
    bool isotropic, jacobian;
};

std::vector<float> linearize_warp(const Warp* warp);

template <typename Array>
Warp delinearize_warp(Array& data);

void delinearize_warp_in_place(float* data, Warp* warp);

//Warp read_warp(std::istream& is);

BRDFData* load_brdf_data(std::string path);
void write_brdf_data(BRDFData* data, std::string path);

bool compare_warp(Warp* warp1, Warp* warp2);
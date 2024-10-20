#include <cmath>
#include <tuple>

constexpr size_t CIEWavelengthStart = 390;
constexpr size_t CIEWavelengthEnd   = 830;
constexpr size_t CIEWavelengthSize  = CIEWavelengthEnd - CIEWavelengthStart + 1;
constexpr float CIEYNorm            = 113.042314572337f; // Integral over CIE_Y
constexpr size_t D65WavelengthStart = 300;
constexpr size_t D65WavelengthEnd   = 830;
constexpr size_t D65WavelengthSize  = (D65WavelengthEnd - D65WavelengthStart) / 5 + 1;

class SpectralMapper {
public:
    /// Maps CIE XYZ to sRGB with D65 as white point
    static inline std::tuple<float, float, float> mapRGB(float x, float y, float z)
    {
        const float r = 3.2404542f * x - 1.5371385f * y - 0.4985314f * z;
        const float g = -0.9692660f * x + 1.8760108f * y + 0.0415560f * z;
        const float b = 0.0556434f * x - 0.2040259f * y + 1.0572252f * z;

        return { std::max(0.0f, r), std::max(0.0f, g), std::max(0.0f, b) };
    }

    /// Returns CIE XYZ color triplet for a given wavelength
    static inline std::tuple<float, float, float> eval(const int* wvls, const float* weights, bool /*emissive*/, size_t size)
    {
        float x = 0, y = 0, z = 0;
        for (size_t i = 0; i < size - 1; ++i) {
            const float aW = (float)wvls[i];
            const float aI = lookupD65(aW);
            const float aV = weights[i];
            const float aX = aI * aV * lookupCIE(aW, CIE_X);
            const float aY = aI * aV * lookupCIE(aW, CIE_Y);
            const float aZ = aI * aV * lookupCIE(aW, CIE_Z);

            const float bW = (float)wvls[i + 1];
            const float bI = lookupD65(bW);
            const float bV = weights[i + 1];
            const float bX = bI * bV * lookupCIE(bW, CIE_X);
            const float bY = bI * bV * lookupCIE(bW, CIE_Y);
            const float bZ = bI * bV * lookupCIE(bW, CIE_Z);

            const float dx = bW - aW;
            x += dx * (bX + aX) / 2;
            y += dx * (bY + aY) / 2;
            z += dx * (bZ + aZ) / 2;
        }

        // if (!emissive) {
        x /= CIEYNormD65;
        y /= CIEYNormD65;
        z /= CIEYNormD65;
        //} else {
        //    x /= CIEYNorm;
        //    y /= CIEYNorm;
        //    z /= CIEYNorm;
        //}

        return { x, y, z };
    }

private:
    static inline float lookupCIE(float wvl, const float* array)
    {
        return lookup(wvl, array, CIEWavelengthStart, CIEWavelengthEnd, CIEWavelengthSize);
    }

    static inline float lookupD65(float wvl)
    {
        return lookup(wvl, CIE_D65, D65WavelengthStart, D65WavelengthEnd, D65WavelengthSize);
    }

    static inline float lookup(float wvl, const float* array, size_t start, size_t end, size_t size)
    {
        if (wvl < start || wvl > end)
            return 0;

        const float delta = (end - start) / (float)(size - 1);
        const float af    = std::max(0.0f, (wvl - start) / delta);
        const int index   = (int)std::min<float>((float)size - 2, af);
        const float t     = std::min<float>((float)size - 1, af) - index;

        return array[index] * (1 - t) + array[index + 1] * t;
    }

    static float computeNormFactorD65();

    static float CIE_X[];
    static float CIE_Y[];
    static float CIE_Z[];
    static float CIE_D65[];
    static float CIEYNormD65;
};
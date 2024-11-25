#pragma once

#include "IG_Config.h"

namespace IG {
class Runtime;
class RenderPass;

class IG_LIB GlareEvaluator {
public:
    GlareEvaluator(Runtime* runtime);
    ~GlareEvaluator();

    struct Result {
        float DGP;
        float DGI;
        float DGImod;
        float DGR;
        float VCP;
        float UGR;
        float UGRexp;
        float UGP;
        float VerticalIlluminance;
        float SourceLuminance;
        float SourceOmega;
        float BackgroundLuminance;
        float TotalOmega;
        float TotalLuminance;
    };
    std::optional<Result> run();

    inline void setMultiplier(float v) { mMultiplier = v; }
    [[nodiscard]] inline float multiplier() const { return mMultiplier; }

    /// @brief Set the vertical illuminance to be used for the evaluation. Set to negative to compute it automatically
    inline void setVerticalIlluminance(float v) { mVerticalIlluminance = v; }
    [[nodiscard]] inline float verticalIlluminance() const { return mVerticalIlluminance; }

    inline void setUserData(const float* data, size_t width, size_t height)
    {
        mData       = data;
        mDataWidth  = width;
        mDataHeight = height;
    }

private:
    void setup();

    Runtime* const mRuntime;
    std::shared_ptr<RenderPass> mPass;

    float mMultiplier;
    float mVerticalIlluminance;

    const float* mData;
    size_t mDataWidth;
    size_t mDataHeight;
};
} // namespace IG
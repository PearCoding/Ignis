#pragma once

#include "Technique.h"

namespace IG {
class InfoBufferTechnique : public Technique {
public:
    InfoBufferTechnique(const Parser::Object& obj);
    ~InfoBufferTechnique() = default;

    TechniqueInfo getInfo(const LoaderContext& ctx) const override;
    void generateBody(const SerializationInput& input) const override;

    /// @brief Will add technique to the end of the variants for denoising or other purposes
    static void enable(TechniqueInfo& info, bool always = false, bool extend = true);

    /// @brief Will insert body definition if the correct variant is set and technique was enabled
    static bool insertBody(const SerializationInput& input, size_t maxDepth, bool followSpecular);

private:
    size_t mMaxDepth;
    bool mFollowSpecular;
    bool mAllIterations;
};
} // namespace IG
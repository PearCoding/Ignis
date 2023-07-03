#pragma once

#include "Technique.h"

namespace IG {
class InfoBufferTechnique {
public:
    /// @brief Will wrap technique around other techniques for denoising or other purposes
    static void enable(TechniqueInfo& info);

    /// @brief Will insert body definition if the correct variant is set and technique was enabled
    static bool insertBody(const Technique::SerializationInput& input);
};
} // namespace IG
#include "Target.h"

namespace IG {
Target TargetInfo::pickCPU()
{
    return Target::GENERIC; // TODO
}

Target TargetInfo::pickGPU()
{
    return Target::NVVM; // TODO
}

} // namespace IG
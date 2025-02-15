#include "IG_Config.h"

namespace IG {
enum class AOVFlags : int {
    None     = 0x0,
    Readonly = 0x1, // AOV will not be modified for the particular access. This is not enforced, but rather a hint.
    Snapshot = 0x2, // AOV will be used every iteration from scratch. The AOV is reset at the beginning of each iteration. The number of samples per pixel is equal to the samples per iteration
    Once     = 0x4  // AOV will be used only once per frame (NOT iteration). The number of samples per pixel is equal to the samples per iteration
};

inline AOVFlags operator|(AOVFlags lhs, AOVFlags rhs)
{
    return static_cast<AOVFlags>(
        static_cast<std::underlying_type<AOVFlags>::type>(lhs) | static_cast<std::underlying_type<AOVFlags>::type>(rhs));
}

inline AOVFlags operator&(AOVFlags lhs, AOVFlags rhs)
{
    return static_cast<AOVFlags>(
        static_cast<std::underlying_type<AOVFlags>::type>(lhs) & static_cast<std::underlying_type<AOVFlags>::type>(rhs));
}

inline AOVFlags operator~(AOVFlags a)
{
    return static_cast<AOVFlags>(~static_cast<std::underlying_type<AOVFlags>::type>(a));
}

} // namespace IG
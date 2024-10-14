#include "Colormapper.h"
#include "Colormap.h"

namespace IG {

Vector4f Colormapper::apply(float v) const
{
    return colormap::plasma(map(v));
}
} // namespace IG
#pragma once

#include "IG_Config.h"

namespace IG {
namespace Tangent {

// Functions applying to the TBN matrix
[[nodiscard]] inline Vector3f fromTangentSpace(const Vector3f& N,
                                 const Vector3f& Nx, const Vector3f& Ny,
                                 const Vector3f& V)
{
    return (N * V(2) + Ny * V(1) + Nx * V(0)).normalized();
}

[[nodiscard]] inline Vector3f toTangentSpace(const Vector3f& N,
                               const Vector3f& Nx, const Vector3f& Ny,
                               const Vector3f& V)
{
    return Vector3f(Nx.dot(V), Ny.dot(V), N.dot(V)).normalized();
}

// N Orientation Z+
/* Frisvad, J. R. (2012). Building an Orthonormal Basis from a 3D Unit Vector Without Normalization.
 * Journal ofGraphics Tools, 16(3), 151–159.
 * https://doi.org/10.1080/2165347X.2012.689606
 * 
 * Nelson Max, Improved accuracy when building an orthonormal basis,
 * Journal of Computer Graphics Techniques (JCGT), vol. 6, no. 1, 9–16, 2017
 * http://jcgt.org/published/0006/01/02/
 */
constexpr float PR_FRAME_FRISVAD_EPS = 0.9999805689f;
inline void frame_frisvad(const Vector3f& N, Vector3f& Nx, Vector3f& Ny)
{
    if (N(2) < -PR_FRAME_FRISVAD_EPS) {
        Nx = Vector3f(0, -1, 0);
        Ny = Vector3f(-1, 0, 0);
    } else {
        const float a = 1.0f / (1.0f + N(2));
        const float b = -N(0) * N(1) * a;
        Nx            = Vector3f(1.0f - N(0) * N(0) * a, b, -N(0));
        Ny            = Vector3f(b, 1.0f - N(1) * N(1) * a, -N(1));
    }
}

/* Tom Duff, James Burgess, Per Christensen, Christophe Hery, Andrew Kensler, Max Liani, and Ryusuke Villemin,
 * Building an Orthonormal Basis, Revisited, Journal of Computer Graphics Techniques (JCGT), vol. 6, no. 1, 1-8, 2017
 * http://jcgt.org/published/0006/01/01/
 */
inline void frame_duff(const Vector3f& N, Vector3f& Nx, Vector3f& Ny)
{
    const float sign = copysignf(1.0f, N(2));
    const float a    = -1.0f / (sign + N(2));
    const float b    = N(0) * N(1) * a;
    Nx               = Vector3f(1.0f + sign * N(0) * N(0) * a, sign * b, -sign * N(0));
    Ny               = Vector3f(b, sign + N(1) * N(1) * a, -N(1));
}

inline void unnormalized_frame(const Vector3f& N, Vector3f& Nx, Vector3f& Ny)
{
#ifdef IG_USE_ORTHOGONAL_FRAME_FRISVAD
    return frame_frisvad(N, Nx, Ny);
#else
    return frame_duff(N, Nx, Ny);
#endif
}

inline void frame(const Vector3f& N, Vector3f& Nx, Vector3f& Ny)
{
    unnormalized_frame(N, Nx, Ny);
    Nx.normalize();
    Ny.normalize();
}

inline void invert_frame(Vector3f& N, Vector3f& Nx, Vector3f& Ny)
{
    N = -N;

    Vector3f t = Nx;
    Nx         = -Ny;
    Ny         = -t;
}

// Align v on N
[[nodiscard]] inline Vector3f align(const Vector3f& N, const Vector3f& V)
{
    Vector3f nx, ny;
    frame(N, nx, ny);
    return fromTangentSpace(N, nx, ny, V);
}

} // namespace Tangent
} // namespace IG

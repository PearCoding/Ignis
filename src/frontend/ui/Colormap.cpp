#include "Colormap.h"

namespace IG::colormap {
static inline Vector4f poly6(const Vector4f& c0, const Vector4f& c1, const Vector4f& c2, const Vector4f& c3, const Vector4f& c4, const Vector4f& c5, const Vector4f& c6, float t)
{
    return c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6)))));
}

Vector4f viridis(float t)
{
    static const Vector4f c0 = Vector4f(0.2777273272234177f, 0.005407344544966578f, 0.3340998053353061f, 1);
    static const Vector4f c1 = Vector4f(0.1050930431085774f, 1.404613529898575f, 1.384590162594685f, 1);
    static const Vector4f c2 = Vector4f(-0.3308618287255563f, 0.214847559468213f, 0.09509516302823659f, 1);
    static const Vector4f c3 = Vector4f(-4.634230498983486f, -5.799100973351585f, -19.33244095627987f, 1);
    static const Vector4f c4 = Vector4f(6.228269936347081f, 14.17993336680509f, 56.69055260068105f, 1);
    static const Vector4f c5 = Vector4f(4.776384997670288f, -13.74514537774601f, -65.35303263337234f, 1);
    static const Vector4f c6 = Vector4f(-5.435455855934631f, 4.645852612178535f, 26.3124352495832f, 1);

    return poly6(c0, c1, c2, c3, c4, c5, c6, t);
}

Vector4f plasma(float t)
{
    static const Vector4f c0 = Vector4f(0.05873234392399702f, 0.02333670892565664f, 0.5433401826748754f, 1);
    static const Vector4f c1 = Vector4f(2.176514634195958f, 0.2383834171260182f, 0.7539604599784036f, 1);
    static const Vector4f c2 = Vector4f(-2.689460476458034f, -7.455851135738909f, 3.110799939717086f, 1);
    static const Vector4f c3 = Vector4f(6.130348345893603f, 42.3461881477227f, -28.51885465332158f, 1);
    static const Vector4f c4 = Vector4f(-11.10743619062271f, -82.66631109428045f, 60.13984767418263f, 1);
    static const Vector4f c5 = Vector4f(10.02306557647065f, 71.41361770095349f, -54.07218655560067f, 1);
    static const Vector4f c6 = Vector4f(-3.658713842777788f, -22.93153465461149f, 18.19190778539828f, 1);

    return poly6(c0, c1, c2, c3, c4, c5, c6, t);
}

Vector4f magma(float t)
{
    static const Vector4f c0 = Vector4f(-0.002136485053939582f, -0.000749655052795221f, -0.005386127855323933f, 1);
    static const Vector4f c1 = Vector4f(0.2516605407371642f, 0.6775232436837668f, 2.494026599312351f, 1);
    static const Vector4f c2 = Vector4f(8.353717279216625f, -3.577719514958484f, 0.3144679030132573f, 1);
    static const Vector4f c3 = Vector4f(-27.66873308576866f, 14.26473078096533f, -13.64921318813922f, 1);
    static const Vector4f c4 = Vector4f(52.17613981234068f, -27.94360607168351f, 12.94416944238394f, 1);
    static const Vector4f c5 = Vector4f(-50.76852536473588f, 29.04658282127291f, 4.23415299384598f, 1);
    static const Vector4f c6 = Vector4f(18.65570506591883f, -11.48977351997711f, -5.601961508734096f, 1);

    return poly6(c0, c1, c2, c3, c4, c5, c6, t);
}

Vector4f inferno(float t)
{
    static const Vector4f c0 = Vector4f(0.0002189403691192265f, 0.001651004631001012f, -0.01948089843709184f, 1);
    static const Vector4f c1 = Vector4f(0.1065134194856116f, 0.5639564367884091f, 3.932712388889277f, 1);
    static const Vector4f c2 = Vector4f(11.60249308247187f, -3.972853965665698f, -15.9423941062914f, 1);
    static const Vector4f c3 = Vector4f(-41.70399613139459f, 17.43639888205313f, 44.35414519872813f, 1);
    static const Vector4f c4 = Vector4f(77.162935699427f, -33.40235894210092f, -81.80730925738993f, 1);
    static const Vector4f c5 = Vector4f(-71.31942824499214f, 32.62606426397723f, 73.20951985803202f, 1);
    static const Vector4f c6 = Vector4f(25.13112622477341f, -12.24266895238567f, -23.07032500287172f, 1);

    return poly6(c0, c1, c2, c3, c4, c5, c6, t);
}
} // namespace IG::colormap
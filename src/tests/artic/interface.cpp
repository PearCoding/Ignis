#include "Logger.h"

using namespace IG;

extern "C" {
struct Vec2 {
    float x, y;
};
struct Vec3 {
    float x, y, z;
};
struct Vec4 {
    float x, y, z, w;
};
struct Mat3x3 {
    Vec3 col[3];
};
struct Mat4x4 {
    Vec4 col[4];
};

int IG_EXPORT ignis_test_expect_eq_f32(float a, float b)
{
    constexpr float EPS = 1.5f;
    const bool s        = std::abs(a - b) <= EPS;
    if (!s) {
        IG_LOG(L_ERROR) << "Expression failed: Expected " << a << " but got " << b << " instead" << std::endl;
        return 1;
    } else {
        return 0;
    }
}
void IG_EXPORT ignis_test_fail(const unsigned char* msg)
{
    IG_LOG(L_ERROR) << "Expression failed: " << reinterpret_cast<const char*>(msg) << std::endl;
}

void IG_EXPORT ignis_dbg_echo_i32(int a)
{
    IG_LOG(L_DEBUG) << a << std::endl;
}

void IG_EXPORT ignis_dbg_echo_f32(float a)
{
    IG_LOG(L_DEBUG) << a << std::endl;
}

void IG_EXPORT ignis_dbg_echo_vec2(Vec2* v)
{
    IG_LOG(L_DEBUG) << "[" << v->x << ", " << v->y << "]" << std::endl;
}

void IG_EXPORT ignis_dbg_echo_vec3(Vec3* v)
{
    IG_LOG(L_DEBUG) << "[" << v->x << ", " << v->y << ", " << v->z << "]" << std::endl;
}

void IG_EXPORT ignis_dbg_echo_vec4(Vec4* v)
{
    IG_LOG(L_DEBUG) << "[" << v->x << ", " << v->y << ", " << v->z << ", " << v->w << "]" << std::endl;
}

void IG_EXPORT ignis_dbg_echo_mat3x3(Mat3x3* m)
{
    IG_LOG(L_DEBUG) << "{" << std::endl
                    << "  [" << m->col[0].x << ", " << m->col[1].x << ", " << m->col[2].x << "]" << std::endl
                    << "  [" << m->col[0].y << ", " << m->col[1].y << ", " << m->col[2].y << "]" << std::endl
                    << "  [" << m->col[0].z << ", " << m->col[1].z << ", " << m->col[2].z << "]" << std::endl
                    << "}" << std::endl;
}

void IG_EXPORT ignis_dbg_echo_mat4x4(Mat4x4* m)
{
    IG_LOG(L_DEBUG) << "{" << std::endl
                    << "  [" << m->col[0].x << ", " << m->col[1].x << ", " << m->col[2].x << ", " << m->col[3].x << "]" << std::endl
                    << "  [" << m->col[0].y << ", " << m->col[1].y << ", " << m->col[2].y << ", " << m->col[3].y << "]" << std::endl
                    << "  [" << m->col[0].z << ", " << m->col[1].z << ", " << m->col[2].z << ", " << m->col[3].z << "]" << std::endl
                    << "  [" << m->col[0].w << ", " << m->col[1].w << ", " << m->col[2].w << ", " << m->col[3].w << "]" << std::endl
                    << "}" << std::endl;
}

int error_count = 0;
void IG_EXPORT ignis_set_error_count(int a)
{
    error_count = a;
}
}
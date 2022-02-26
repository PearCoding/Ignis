#include <iostream>

#include "generated_test_interface.h"

extern "C" {
int ignis_test_expect_eq_f32(float a, float b)
{
    constexpr float EPS = 1.5f;
    const bool s        = std::abs(a - b) <= EPS;
    if (!s) {
        std::cout << "Expression failed: Expected " << b << " but got " << a << " instead" << std::endl;
        return 1;
    } else {
        return 0;
    }
}
void ignis_test_fail(unsigned char* msg)
{
    std::cout << "Expression failed: " << reinterpret_cast<const char*>(msg) << std::endl;
}

void ignis_dbg_echo_i32(int a)
{
    std::cout << "Debug: " << a << std::endl;
}

void ignis_dbg_echo_f32(float a)
{
    std::cout << "Debug: " << a << std::endl;
}

void ignis_dbg_echo_vec2(Vec2* v)
{
    std::cout << "Debug: [" << v->x << ", " << v->y << "]" << std::endl;
}

void ignis_dbg_echo_vec3(Vec3* v)
{
    std::cout << "Debug: [" << v->x << ", " << v->y << ", " << v->z << "]" << std::endl;
}

void ignis_dbg_echo_vec4(Vec4* v)
{
    std::cout << "Debug: [" << v->x << ", " << v->y << ", " << v->z << ", " << v->w << "]" << std::endl;
}

void ignis_dbg_echo_mat3x3(Mat3x3* m)
{
    std::cout << "Debug: {" << std::endl;
    std::cout << "  [" << m->col.e[0].x << ", " << m->col.e[1].x << ", " << m->col.e[2].x << "]" << std::endl;
    std::cout << "  [" << m->col.e[0].y << ", " << m->col.e[1].y << ", " << m->col.e[2].y << "]" << std::endl;
    std::cout << "  [" << m->col.e[0].z << ", " << m->col.e[1].z << ", " << m->col.e[2].z << "]" << std::endl;
    std::cout << "}" << std::endl;
}

void ignis_dbg_echo_mat4x4(Mat4x4* m)
{
    std::cout << "Debug: {" << std::endl;
    std::cout << "  [" << m->col.e[0].x << ", " << m->col.e[1].x << ", " << m->col.e[2].x << ", " << m->col.e[3].x << "]" << std::endl;
    std::cout << "  [" << m->col.e[0].y << ", " << m->col.e[1].y << ", " << m->col.e[2].y << ", " << m->col.e[3].y << "]" << std::endl;
    std::cout << "  [" << m->col.e[0].z << ", " << m->col.e[1].z << ", " << m->col.e[2].z << ", " << m->col.e[3].z << "]" << std::endl;
    std::cout << "  [" << m->col.e[0].w << ", " << m->col.e[1].w << ", " << m->col.e[2].w << ", " << m->col.e[3].w << "]" << std::endl;
    std::cout << "}" << std::endl;
}
}
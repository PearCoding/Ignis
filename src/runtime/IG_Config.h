#pragma once

#define IG_STRINGIFY(str) #str
#define IG_DOUBLEQUOTE(str) IG_STRINGIFY(str)

// CPU
#if defined(__arm__) || defined(_M_ARM)
#define IG_CPU_ARM
#elif defined(__x86_64__) || defined(_M_X64) || defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
#define IG_CPU_X86
#else
#define IG_CPU_UNKNOWN
#endif

// OS
#if defined(__linux) || defined(linux)
#define IG_OS_LINUX
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__) || defined(__TOS_WIN__)
#define IG_OS_WINDOWS
#if !defined(Win64) && !defined(_WIN64)
#define IG_OS_WINDOWS_32
#else
#define IG_OS_WINDOWS_64
#endif
#elif defined(__APPLE__)
#define IG_OS_APPLE
#else
#warning Your operating system is currently not supported
#endif

// Compiler
#if defined(__CYGWIN__)
#define IG_CC_CYGWIN
#endif

#if defined(__GNUC__)
#define IG_CC_GNU
#endif

#if defined(__clang__)
#define IG_CC_CLANG
#endif

#if defined(__MINGW32__)
#define IG_CC_MINGW32
#endif

#if defined(__INTEL_COMPILER)
#define IG_CC_INTEL
#endif

#if defined(_MSC_VER)
#define IG_CC_MSC
#pragma warning(disable : 4251 4996)
#endif

// Check if C++17
#ifdef IG_CC_MSC
#define IG_CPP_LANG _MSVC_LANG
#else
#define IG_CPP_LANG __cplusplus
#endif

#if IG_CPP_LANG < 201703L
#pragma warning Ignis requires C++ 17 to compile successfully
#endif

#define IG_PRAGMA(x) _Pragma(#x)

// clang-format off
#define IG_UNUSED(expr) do { (void)(expr); } while (false)
#define IG_NOOP do {} while(false)
// clang-format on

#ifdef IG_CC_MSC
#define IG_DEBUG_BREAK() __debugbreak()
#define IG_FUNCTION_NAME __FUNCSIG__
#define IG_BEGIN_IGNORE_WARNINGS IG_PRAGMA(warning(push, 0))
#define IG_END_IGNORE_WARNINGS IG_PRAGMA(warning(pop))
#else
#if defined(__x86_64__) || defined(__i386__)
#define IG_DEBUG_BREAK() __asm__ __volatile__("int $0x03")
#else
#define IG_DEBUG_BREAK() IG_NOOP
#endif
#define IG_FUNCTION_NAME __PRETTY_FUNCTION__
#if defined(IG_CC_GNU)
#define IG_BEGIN_IGNORE_WARNINGS                          \
    IG_PRAGMA(GCC diagnostic push)                        \
    IG_PRAGMA(GCC diagnostic ignored "-Wunused-variable") \
    IG_PRAGMA(GCC diagnostic ignored "-Wmissing-field-initializers")
#define IG_END_IGNORE_WARNINGS IG_PRAGMA(GCC diagnostic pop)
#elif defined(IG_CC_CLANG)
#define IG_BEGIN_IGNORE_WARNINGS                            \
    IG_PRAGMA(clang diagnostic push)                        \
    IG_PRAGMA(clang diagnostic ignored "-Wunused-variable") \
    IG_PRAGMA(clang diagnostic ignored "-Wmissing-field-initializers")
#define IG_END_IGNORE_WARNINGS IG_PRAGMA(clang diagnostic pop)
#else
#define IG_BEGIN_IGNORE_WARNINGS
#define IG_END_IGNORE_WARNINGS
#endif
#endif

#if defined(IG_CC_GNU) || defined(IG_CC_CLANG)
#define IG_LIKELY(x) __builtin_expect(!!(x), 1)
#define IG_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define IG_LIKELY(x) (x)
#define IG_UNLIKELY(x) (x)
#endif

#if defined(IG_WITH_ASSERTS) || (!defined(IG_NO_ASSERTS) && defined(IG_DEBUG))
#include <assert.h>
#define _IG_ASSERT_MSG(msg)                                 \
    std::cerr << "[IGNIS] ASSERT | " << __FILE__            \
              << ":" << __LINE__ << " " << IG_FUNCTION_NAME \
              << " | " << (msg) << std::endl
#ifndef IG_DEBUG
#define IG_ASSERT(cond, msg)        \
    do {                            \
        if (IG_UNLIKELY(!(cond))) { \
            _IG_ASSERT_MSG((msg));  \
            std::abort();           \
        }                           \
    } while (false)
#else
#define IG_ASSERT(cond, msg)        \
    do {                            \
        if (IG_UNLIKELY(!(cond))) { \
            _IG_ASSERT_MSG((msg));  \
            IG_DEBUG_BREAK();       \
            std::abort();           \
        }                           \
    } while (false)
#endif
#else
#define IG_ASSERT(cond, msg) ((void)0)
#endif

#define IG_CLASS_NON_MOVEABLE(C) \
private:                         \
    C(C&&)            = delete;  \
    C& operator=(C&&) = delete

#define IG_CLASS_NON_COPYABLE(C)     \
private:                             \
    C(const C&)            = delete; \
    C& operator=(const C&) = delete

#define IG_CLASS_NON_CONSTRUCTABLE(C) \
private:                              \
    C() = delete

#if defined(IG_CC_MSC)
#define IG_EXPORT __declspec(dllexport)
#define IG_IMPORT __declspec(dllimport)
#elif defined(IG_CC_GNU) || defined(IG_CC_CLANG)
#define IG_EXPORT __attribute__((visibility("default")))
#define IG_IMPORT
#else
#error Unsupported compiler
#endif

#if defined(IG_BUILD_LIB)
#define IG_LIB IG_EXPORT
#else
#define IG_LIB IG_IMPORT
#endif

// Useful includes
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

// Eigen3 Library
#include <Eigen/Dense>
#include <Eigen/Geometry>

namespace IG {
/* The intx_t are optional but our system demands them.
 * A system without these types can not use the raytracer
 * due to performance issues anyway.
 */

using int8  = int8_t;
using uint8 = uint8_t;

using int16  = int16_t;
using uint16 = uint16_t;

using int32  = int32_t;
using uint32 = uint32_t;

using int64  = int64_t;
using uint64 = uint64_t;

/* Checks */
static_assert(sizeof(int8) == 1, "Invalid bytesize configuration");
static_assert(sizeof(uint8) == 1, "Invalid bytesize configuration");
static_assert(sizeof(int16) == 2, "Invalid bytesize configuration");
static_assert(sizeof(uint16) == 2, "Invalid bytesize configuration");
static_assert(sizeof(int32) == 4, "Invalid bytesize configuration");
static_assert(sizeof(uint32) == 4, "Invalid bytesize configuration");
static_assert(sizeof(int64) == 8, "Invalid bytesize configuration");
static_assert(sizeof(uint64) == 8, "Invalid bytesize configuration");

/* Precise vector types */
using Vector2f = Eigen::Vector2f;
using Vector2i = Eigen::Vector2i;

using Vector3f = Eigen::Vector3f;
using Vector3i = Eigen::Vector3i;

using Vector4f = Eigen::Vector4f;
using Vector4i = Eigen::Vector4i;

/* Precise matrix types */
using Matrix2f = Eigen::Matrix2f;
using Matrix3f = Eigen::Matrix3f;
using Matrix4f = Eigen::Matrix4f;

using Quaternionf = Eigen::Quaternionf;
using Transformf  = Eigen::Affine3f;

using Colorf = Eigen::Array3f;

template <typename Key, typename VectorType>
using AlignedUnorderedMap = std::unordered_map<Key, VectorType, std::hash<Key>, std::equal_to<Key>, Eigen::aligned_allocator<std::pair<const Key, VectorType>>>;

/* Useful constants */
constexpr float FltEps = std::numeric_limits<float>::epsilon();
constexpr float FltInf = std::numeric_limits<float>::infinity();
constexpr float FltMax = std::numeric_limits<float>::max();

constexpr float Pi     = 3.14159265358979323846f;
constexpr float InvPi  = 0.31830988618379067154f; // 1/pi
constexpr float Inv2Pi = 0.15915494309189533577f; // 1/(2pi)
constexpr float Inv4Pi = 0.07957747154594766788f; // 1/(4pi)
constexpr float Pi2    = 1.57079632679489661923f; // pi half
constexpr float Pi4    = 0.78539816339744830961f; // pi quarter
constexpr float Sqrt2  = 1.41421356237309504880f;

constexpr float Deg2Rad = Pi / 180.0f;
constexpr float Rad2Deg = 180.0f * InvPi;

/// Clamps a between b and c.
template <typename T>
[[nodiscard]] inline T clamp(const T& a, const T& b, const T& c)
{
    return (a < b) ? b : ((a > c) ? c : a);
}

} // namespace IG
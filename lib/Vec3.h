// Some General Notes here
/*
* We should probably delete the functions we wont use after the Firmware is finished
* This is meant for other C files but should work in C++, untested
*/

#pragma once
/*
* vec3.h - lightweight 3-component vector for Arsenal3 firmware
*
* - Plain aggregate struct (no vtable, no heap, trivially copyable), so it
*   can be memcpy'd, put in packed buffers, or passed through FreeRTOS queues.
* - Templated on the component type; Vec3f (float) is the default workhorse
*   because the ESP32 FPU is single precision (double is emulated in software).
* - Operators are "hidden friends", so mixed scalars work: v * 2, 0.5f * v.
*
*   Vec3f a{1.0f, 2.0f, 3.0f};
*   Vec3f b = Vec3f::up();
*   auto  c = cross(a, b);
*   float d = dot(a, b);
*   auto  n = normalize(a);
*/

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>

template <typename T>
struct Vec3
{
    static_assert(std::is_arithmetic<T>::value, "Vec3<T> requires an arithmetic type");

    using value_type = T;
    /* Type used for results that need a real number (length, angle, ...).
       Integer vectors fall back to float. */
    using real_type = typename std::conditional<std::is_floating_point<T>::value, T, float>::type;

    T x = 0;
    T y = 0;
    T z = 0;

    /* ---- constants -------------------------------------------------- */
    static constexpr Vec3 zero()    { return {0, 0, 0}; }
    static constexpr Vec3 one()     { return {1, 1, 1}; }
    static constexpr Vec3 unitX()   { return {1, 0, 0}; }
    static constexpr Vec3 unitY()   { return {0, 1, 0}; }
    static constexpr Vec3 unitZ()   { return {0, 0, 1}; }
    static constexpr Vec3 up()      { return unitZ(); }   /* Z-up convention */
    static constexpr Vec3 forward() { return unitX(); }
    static constexpr Vec3 right()   { return {0, static_cast<T>(-1), 0}; }

    /* ---- element access (index 0..2, no bounds check beyond clamping) - */
    constexpr T &operator[](size_t i)             { return i == 0 ? x : (i == 1 ? y : z); }
    constexpr const T &operator[](size_t i) const { return i == 0 ? x : (i == 1 ? y : z); }

    /* ---- compound assignment ---------------------------------------- */
    constexpr Vec3 &operator+=(const Vec3 &o) { x += o.x; y += o.y; z += o.z; return *this; }
    constexpr Vec3 &operator-=(const Vec3 &o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    constexpr Vec3 &operator*=(const Vec3 &o) { x *= o.x; y *= o.y; z *= o.z; return *this; }
    constexpr Vec3 &operator/=(const Vec3 &o) { x /= o.x; y /= o.y; z /= o.z; return *this; }
    constexpr Vec3 &operator*=(T s)           { x *= s; y *= s; z *= s; return *this; }
    constexpr Vec3 &operator/=(T s)           { x /= s; y /= s; z /= s; return *this; }

    /* ---- arithmetic (component-wise for vec op vec) ------------------ */
    friend constexpr Vec3 operator+(Vec3 a, const Vec3 &b) { return a += b; }
    friend constexpr Vec3 operator-(Vec3 a, const Vec3 &b) { return a -= b; }
    friend constexpr Vec3 operator*(Vec3 a, const Vec3 &b) { return a *= b; }
    friend constexpr Vec3 operator/(Vec3 a, const Vec3 &b) { return a /= b; }

    friend constexpr Vec3 operator*(Vec3 v, T s) { return v *= s; }
    friend constexpr Vec3 operator*(T s, Vec3 v) { return v *= s; }
    friend constexpr Vec3 operator/(Vec3 v, T s) { return v /= s; }

    friend constexpr Vec3 operator-(const Vec3 &v) { return {static_cast<T>(-v.x), static_cast<T>(-v.y), static_cast<T>(-v.z)}; }
    friend constexpr Vec3 operator+(const Vec3 &v) { return v; }

    /* ---- exact comparison (use nearlyEqual() for floats) ------------- */
    friend constexpr bool operator==(const Vec3 &a, const Vec3 &b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
    friend constexpr bool operator!=(const Vec3 &a, const Vec3 &b) { return !(a == b); }
};

using Vec3f = Vec3<float>;
using Vec3d = Vec3<double>;
using Vec3i = Vec3<int32_t>;

/* ======================================================================
* Free functions
* ==================================================================== */

/* Convert between component types: vec3_cast<int32_t>(someVec3f) */
template <typename U, typename T>
constexpr Vec3<U> vec3_cast(const Vec3<T> &v)
{
    return {static_cast<U>(v.x), static_cast<U>(v.y), static_cast<U>(v.z)};
}

template <typename T>
constexpr T dot(const Vec3<T> &a, const Vec3<T> &b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

template <typename T>
constexpr Vec3<T> cross(const Vec3<T> &a, const Vec3<T> &b)
{
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

/* Squared length: cheap, no sqrt. Prefer this for comparisons. */
template <typename T>
constexpr T lengthSq(const Vec3<T> &v)
{
    return dot(v, v);
}

template <typename T>
inline typename Vec3<T>::real_type length(const Vec3<T> &v)
{
    using R = typename Vec3<T>::real_type;
    return std::sqrt(static_cast<R>(lengthSq(v)));
}

template <typename T>
constexpr T distanceSq(const Vec3<T> &a, const Vec3<T> &b)
{
    return lengthSq(b - a);
}

template <typename T>
inline typename Vec3<T>::real_type distance(const Vec3<T> &a, const Vec3<T> &b)
{
    return length(b - a);
}

/* Returns a unit vector, or zero if the input is (near) zero length. */
template <typename T>
inline Vec3<T> normalize(const Vec3<T> &v, T epsilon = static_cast<T>(1e-6))
{
    static_assert(std::is_floating_point<T>::value, "normalize() needs a floating-point Vec3");
    const T len = length(v);
    return (len > epsilon) ? v / len : Vec3<T>::zero();
}

template <typename T>
constexpr bool nearlyEqual(const Vec3<T> &a, const Vec3<T> &b, T epsilon = static_cast<T>(1e-5))
{
    return (a.x - b.x <= epsilon && b.x - a.x <= epsilon) &&
           (a.y - b.y <= epsilon && b.y - a.y <= epsilon) &&
           (a.z - b.z <= epsilon && b.z - a.z <= epsilon);
}

/* Linear interpolation: t = 0 -> a, t = 1 -> b */
template <typename T>
constexpr Vec3<T> lerp(const Vec3<T> &a, const Vec3<T> &b, T t)
{
    return a + (b - a) * t;
}

template <typename T>
constexpr Vec3<T> min(const Vec3<T> &a, const Vec3<T> &b)
{
    return {a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y, a.z < b.z ? a.z : b.z};
}

template <typename T>
constexpr Vec3<T> max(const Vec3<T> &a, const Vec3<T> &b)
{
    return {a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y, a.z > b.z ? a.z : b.z};
}

template <typename T>
constexpr Vec3<T> clamp(const Vec3<T> &v, const Vec3<T> &lo, const Vec3<T> &hi)
{
    return min(max(v, lo), hi);
}

template <typename T>
constexpr Vec3<T> abs(const Vec3<T> &v)
{
    return {v.x < 0 ? static_cast<T>(-v.x) : v.x,
            v.y < 0 ? static_cast<T>(-v.y) : v.y,
            v.z < 0 ? static_cast<T>(-v.z) : v.z};
}

/* Clamp the vector's length to maxLen, keeping its direction. */
template <typename T>
inline Vec3<T> clampLength(const Vec3<T> &v, T maxLen)
{
    static_assert(std::is_floating_point<T>::value, "clampLength() needs a floating-point Vec3");
    const T lenSq = lengthSq(v);
    if (lenSq > maxLen * maxLen && lenSq > 0)
        return v * (maxLen / std::sqrt(lenSq));
    return v;
}

/* Projection of v onto `onto` (onto does not need to be normalized). */
template <typename T>
inline Vec3<T> project(const Vec3<T> &v, const Vec3<T> &onto)
{
    static_assert(std::is_floating_point<T>::value, "project() needs a floating-point Vec3");
    const T d = lengthSq(onto);
    return (d > 0) ? onto * (dot(v, onto) / d) : Vec3<T>::zero();
}

/* Component of v perpendicular to `onto`. */
template <typename T>
inline Vec3<T> reject(const Vec3<T> &v, const Vec3<T> &onto)
{
    return v - project(v, onto);
}

/* Reflect v about a surface with the given (unit-length) normal. */
template <typename T>
constexpr Vec3<T> reflect(const Vec3<T> &v, const Vec3<T> &normal)
{
    return v - normal * (static_cast<T>(2) * dot(v, normal));
}

/* Unsigned angle between two vectors, in radians [0, pi]. */
template <typename T>
inline typename Vec3<T>::real_type angle(const Vec3<T> &a, const Vec3<T> &b)
{
    using R = typename Vec3<T>::real_type;
    const R denom = length(a) * length(b);
    if (denom <= static_cast<R>(0))
        return 0;
    R c = static_cast<R>(dot(a, b)) / denom;
    c = c < static_cast<R>(-1) ? static_cast<R>(-1) : (c > static_cast<R>(1) ? static_cast<R>(1) : c);
    return std::acos(c);
}
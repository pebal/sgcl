//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/detail/os.h"
#include "../core/slice.h"
#include "geometry.h"

#include <cmath>
#include <cstddef>

#if defined(__aarch64__) && defined(__ARM_NEON) && !defined(SGCL_MATH_PORTABLE)
#include <arm_neon.h>
#define SGCL_MATH_NEON 1
#elif defined(__x86_64__) && defined(__SSE2__) && !defined(SGCL_MATH_PORTABLE)
#include <emmintrin.h>
#define SGCL_MATH_SSE2 1
#endif

// The small algebra of a 3D scene and of a GPU: vectors of two, three and
// four floats, 3×3 and 4×4 matrices and quaternions — what GLSL, Metal and
// glm have, as plain values. Column vectors and column-major matrices:
// m * v is the matrix applied to the vector, a * b applies b first, and
// data() is the sixteen floats a shader takes as they are. The projections
// are right-handed (the camera looks down -z) with depth from 0 to 1 in
// clip space, as Metal, Vulkan, Direct3D and WebGPU clip; OpenGL's -1 to 1
// is the one that needs a fixed matrix in front.
//
// Float throughout: what the GPU and the plane's shapes (geometry.h) take.
// Nothing here throws or allocates; an inverse of a singular matrix is
// nothing, a zero vector normalized stays zero, NaN goes through as IEEE
// arithmetic carries it. mat4's product and its batch apply() are written
// on NEON (and SSE2, the x86-64 minimum) where that measured faster than
// the loop the compiler makes of the plain code; SGCL_MATH_PORTABLE leaves
// the plain code alone, for the test that holds the two against each other.
namespace sgcl::math {
    struct vec2 {
        float x = 0;
        float y = 0;

        constexpr vec2() noexcept = default;

        SGCL_INLINE_HOT constexpr vec2(float x, float y) noexcept
        : x(x)
        , y(y) {
        }

        // A point as a vector, and back: explicitly, the two meaning
        // different things (a place of the plane, a quantity of the algebra)
        SGCL_INLINE_HOT constexpr explicit vec2(const point& p) noexcept
        : x(p.x)
        , y(p.y) {
        }

        SGCL_INLINE_HOT constexpr explicit operator point() const noexcept {
            return {x, y};
        }

        SGCL_INLINE_HOT constexpr float dot(const vec2& v) const noexcept {
            return x * v.x + y * v.y;
        }

        // The z of the cross product of the two in 3D: the signed area of
        // the parallelogram they span, positive when v turns left of this
        SGCL_INLINE_HOT constexpr float cross(const vec2& v) const noexcept {
            return x * v.y - y * v.x;
        }

        SGCL_INLINE_HOT constexpr float length_squared() const noexcept {
            return dot(*this);
        }

        SGCL_INLINE_HOT float length() const noexcept {
            return std::sqrt(length_squared());
        }

        SGCL_INLINE_HOT float distance(const vec2& v) const noexcept {
            return vec2(x - v.x, y - v.y).length();
        }

        // Of length one in the same direction; a zero vector stays zero
        SGCL_INLINE_HOT vec2 normalized() const noexcept {
            float l = length();
            return l > 0 ? vec2(x / l, y / l) : *this;
        }

        SGCL_INLINE_HOT friend constexpr vec2 operator+(const vec2& a, const vec2& b) noexcept {
            return {a.x + b.x, a.y + b.y};
        }

        SGCL_INLINE_HOT friend constexpr vec2 operator-(const vec2& a, const vec2& b) noexcept {
            return {a.x - b.x, a.y - b.y};
        }

        // Componentwise, as GLSL multiplies two vectors
        SGCL_INLINE_HOT friend constexpr vec2 operator*(const vec2& a, const vec2& b) noexcept {
            return {a.x * b.x, a.y * b.y};
        }

        SGCL_INLINE_HOT friend constexpr vec2 operator*(const vec2& a, float k) noexcept {
            return {a.x * k, a.y * k};
        }

        SGCL_INLINE_HOT friend constexpr vec2 operator*(float k, const vec2& a) noexcept {
            return {a.x * k, a.y * k};
        }

        SGCL_INLINE_HOT friend constexpr vec2 operator/(const vec2& a, float k) noexcept {
            return {a.x / k, a.y / k};
        }

        SGCL_INLINE_HOT constexpr vec2 operator-() const noexcept {
            return {-x, -y};
        }

        SGCL_INLINE_HOT constexpr vec2& operator+=(const vec2& b) noexcept {
            return *this = *this + b;
        }

        SGCL_INLINE_HOT constexpr vec2& operator-=(const vec2& b) noexcept {
            return *this = *this - b;
        }

        SGCL_INLINE_HOT constexpr vec2& operator*=(float k) noexcept {
            return *this = *this * k;
        }

        SGCL_INLINE_HOT constexpr vec2& operator/=(float k) noexcept {
            return *this = *this / k;
        }

        friend constexpr bool operator==(const vec2&, const vec2&) noexcept = default;
    };

    struct vec3 {
        float x = 0;
        float y = 0;
        float z = 0;

        constexpr vec3() noexcept = default;

        SGCL_INLINE_HOT constexpr vec3(float x, float y, float z) noexcept
        : x(x)
        , y(y)
        , z(z) {
        }

        SGCL_INLINE_HOT constexpr vec3(const vec2& v, float z) noexcept
        : x(v.x)
        , y(v.y)
        , z(z) {
        }

        SGCL_INLINE_HOT constexpr float dot(const vec3& v) const noexcept {
            return x * v.x + y * v.y + z * v.z;
        }

        // Perpendicular to both, by the right-hand rule: x × y is z
        SGCL_INLINE_HOT constexpr vec3 cross(const vec3& v) const noexcept {
            return {y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x};
        }

        SGCL_INLINE_HOT constexpr float length_squared() const noexcept {
            return dot(*this);
        }

        SGCL_INLINE_HOT float length() const noexcept {
            return std::sqrt(length_squared());
        }

        SGCL_INLINE_HOT float distance(const vec3& v) const noexcept {
            return vec3(x - v.x, y - v.y, z - v.z).length();
        }

        SGCL_INLINE_HOT vec3 normalized() const noexcept {
            float l = length();
            return l > 0 ? vec3(x / l, y / l, z / l) : *this;
        }

        SGCL_INLINE_HOT friend constexpr vec3 operator+(const vec3& a, const vec3& b) noexcept {
            return {a.x + b.x, a.y + b.y, a.z + b.z};
        }

        SGCL_INLINE_HOT friend constexpr vec3 operator-(const vec3& a, const vec3& b) noexcept {
            return {a.x - b.x, a.y - b.y, a.z - b.z};
        }

        SGCL_INLINE_HOT friend constexpr vec3 operator*(const vec3& a, const vec3& b) noexcept {
            return {a.x * b.x, a.y * b.y, a.z * b.z};
        }

        SGCL_INLINE_HOT friend constexpr vec3 operator*(const vec3& a, float k) noexcept {
            return {a.x * k, a.y * k, a.z * k};
        }

        SGCL_INLINE_HOT friend constexpr vec3 operator*(float k, const vec3& a) noexcept {
            return {a.x * k, a.y * k, a.z * k};
        }

        SGCL_INLINE_HOT friend constexpr vec3 operator/(const vec3& a, float k) noexcept {
            return {a.x / k, a.y / k, a.z / k};
        }

        SGCL_INLINE_HOT constexpr vec3 operator-() const noexcept {
            return {-x, -y, -z};
        }

        SGCL_INLINE_HOT constexpr vec3& operator+=(const vec3& b) noexcept {
            return *this = *this + b;
        }

        SGCL_INLINE_HOT constexpr vec3& operator-=(const vec3& b) noexcept {
            return *this = *this - b;
        }

        SGCL_INLINE_HOT constexpr vec3& operator*=(float k) noexcept {
            return *this = *this * k;
        }

        SGCL_INLINE_HOT constexpr vec3& operator/=(float k) noexcept {
            return *this = *this / k;
        }

        friend constexpr bool operator==(const vec3&, const vec3&) noexcept = default;
    };

    struct vec4 {
        float x = 0;
        float y = 0;
        float z = 0;
        float w = 0;

        constexpr vec4() noexcept = default;

        SGCL_INLINE_HOT constexpr vec4(float x, float y, float z, float w) noexcept
        : x(x)
        , y(y)
        , z(z)
        , w(w) {
        }

        SGCL_INLINE_HOT constexpr vec4(const vec3& v, float w) noexcept
        : x(v.x)
        , y(v.y)
        , z(v.z)
        , w(w) {
        }

        // The first three
        SGCL_INLINE_HOT constexpr vec3 xyz() const noexcept {
            return {x, y, z};
        }

        SGCL_INLINE_HOT constexpr float dot(const vec4& v) const noexcept {
            return x * v.x + y * v.y + z * v.z + w * v.w;
        }

        SGCL_INLINE_HOT constexpr float length_squared() const noexcept {
            return dot(*this);
        }

        SGCL_INLINE_HOT float length() const noexcept {
            return std::sqrt(length_squared());
        }

        SGCL_INLINE_HOT float distance(const vec4& v) const noexcept {
            return vec4(x - v.x, y - v.y, z - v.z, w - v.w).length();
        }

        SGCL_INLINE_HOT vec4 normalized() const noexcept {
            float l = length();
            return l > 0 ? vec4(x / l, y / l, z / l, w / l) : *this;
        }

        SGCL_INLINE_HOT friend constexpr vec4 operator+(const vec4& a, const vec4& b) noexcept {
            return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
        }

        SGCL_INLINE_HOT friend constexpr vec4 operator-(const vec4& a, const vec4& b) noexcept {
            return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
        }

        SGCL_INLINE_HOT friend constexpr vec4 operator*(const vec4& a, const vec4& b) noexcept {
            return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w};
        }

        SGCL_INLINE_HOT friend constexpr vec4 operator*(const vec4& a, float k) noexcept {
            return {a.x * k, a.y * k, a.z * k, a.w * k};
        }

        SGCL_INLINE_HOT friend constexpr vec4 operator*(float k, const vec4& a) noexcept {
            return {a.x * k, a.y * k, a.z * k, a.w * k};
        }

        SGCL_INLINE_HOT friend constexpr vec4 operator/(const vec4& a, float k) noexcept {
            return {a.x / k, a.y / k, a.z / k, a.w / k};
        }

        SGCL_INLINE_HOT constexpr vec4 operator-() const noexcept {
            return {-x, -y, -z, -w};
        }

        SGCL_INLINE_HOT constexpr vec4& operator+=(const vec4& b) noexcept {
            return *this = *this + b;
        }

        SGCL_INLINE_HOT constexpr vec4& operator-=(const vec4& b) noexcept {
            return *this = *this - b;
        }

        SGCL_INLINE_HOT constexpr vec4& operator*=(float k) noexcept {
            return *this = *this * k;
        }

        SGCL_INLINE_HOT constexpr vec4& operator/=(float k) noexcept {
            return *this = *this / k;
        }

        friend constexpr bool operator==(const vec4&, const vec4&) noexcept = default;
    };

    static_assert(sizeof(vec4) == 16 && sizeof(vec3) == 12 && sizeof(vec2) == 8);

    // A 3×3 matrix, column-major: columns[c] is column c, and (row, column)
    // reads one number. The identity by default. As a 2D transform in
    // homogeneous coordinates it is an affine with a last row of 0 0 1.
    struct mat3 {
        vec3 columns[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

        constexpr mat3() noexcept = default;

        SGCL_INLINE_HOT constexpr mat3(const vec3& c0, const vec3& c1, const vec3& c2) noexcept
        : columns{c0, c1, c2} {
        }

        // The affine transform: a c e / b d f / 0 0 1
        SGCL_INLINE_HOT constexpr explicit mat3(const affine& t) noexcept
        : columns{{t.a, t.b, 0}, {t.c, t.d, 0}, {t.e, t.f, 1}} {
        }

        SGCL_INLINE_HOT static constexpr mat3 identity() noexcept {
            return {};
        }

        SGCL_INLINE_HOT constexpr float operator()(int row, int column) const noexcept {
            const vec3& c = columns[column];
            return row == 0 ? c.x : row == 1 ? c.y : c.z;
        }

        SGCL_INLINE_HOT friend constexpr vec3 operator*(const mat3& m, const vec3& v) noexcept {
            return m.columns[0] * v.x + m.columns[1] * v.y + m.columns[2] * v.z;
        }

        // b first, then a
        SGCL_INLINE_HOT friend constexpr mat3 operator*(const mat3& a, const mat3& b) noexcept {
            return {a * b.columns[0], a * b.columns[1], a * b.columns[2]};
        }

        SGCL_INLINE_HOT constexpr mat3& operator*=(const mat3& b) noexcept {
            return *this = *this * b;
        }

        SGCL_INLINE_HOT constexpr mat3 transposed() const noexcept {
            const vec3* c = columns;
            return {{c[0].x, c[1].x, c[2].x}, {c[0].y, c[1].y, c[2].y}, {c[0].z, c[1].z, c[2].z}};
        }

        SGCL_INLINE_HOT constexpr float determinant() const noexcept {
            return columns[0].dot(columns[1].cross(columns[2]));
        }

        // The inverse, from the cofactors; nothing for a determinant of zero
        optional<mat3> inverse() const noexcept {
            const vec3* c = columns;
            vec3 r0 = c[1].cross(c[2]);
            vec3 r1 = c[2].cross(c[0]);
            vec3 r2 = c[0].cross(c[1]);
            float det = c[0].dot(r0);
            if (det == 0 || !std::isfinite(det)) {
                return nullopt;
            }
            // rows of the inverse are the cross products over the determinant
            mat3 rows(r0 / det, r1 / det, r2 / det);
            return rows.transposed();
        }

        // Nine floats, column after column, as a GPU takes them
        SGCL_INLINE_HOT const float* data() const noexcept {
            return &columns[0].x;
        }

        friend constexpr bool operator==(const mat3&, const mat3&) noexcept = default;
    };

    struct quaternion;

    // A 4×4 matrix, column-major, the identity by default: the transforms of
    // 3D space in homogeneous coordinates and the projections
    struct mat4 {
        vec4 columns[4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};

        constexpr mat4() noexcept = default;

        SGCL_INLINE_HOT constexpr mat4(const vec4& c0, const vec4& c1, const vec4& c2, const vec4& c3) noexcept
        : columns{c0, c1, c2, c3} {
        }

        SGCL_INLINE_HOT static constexpr mat4 identity() noexcept {
            return {};
        }

        SGCL_INLINE_HOT static constexpr mat4 translation(const vec3& offset) noexcept {
            return {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {offset, 1}};
        }

        SGCL_INLINE_HOT static constexpr mat4 scaling(const vec3& factors) noexcept {
            return {{factors.x, 0, 0, 0}, {0, factors.y, 0, 0}, {0, 0, factors.z, 0}, {0, 0, 0, 1}};
        }

        // A turn about the axis (normalized here; a zero axis is the
        // identity), counter-clockwise looking from its tip towards the origin
        static mat4 rotation(const vec3& axis, float radians) noexcept;

        // The view of a camera at eye looking at target, up the direction
        // that shows up: right-handed, the camera looking down its -z
        static mat4 look_at(const vec3& eye, const vec3& target, const vec3& up) noexcept;

        // A perspective projection: fov_y the vertical angle of view,
        // aspect the width over the height, z_near and z_far the distances
        // of the planes (above zero); depth 0 at z_near and 1 at z_far. A
        // z_far of infinity is the infinite projection. (Not near and far:
        // windows.h defines both as macros.)
        static mat4 perspective(float fov_y, float aspect, float z_near, float z_far) noexcept;

        // A parallel projection of the box onto clip space, depth 0 at
        // -z_near and 1 at -z_far (right-handed, as perspective)
        SGCL_INLINE_HOT static constexpr mat4 orthographic(float left, float right, float bottom, float top, float z_near,
                                                           float z_far) noexcept {
            return {{2 / (right - left), 0, 0, 0},
                    {0, 2 / (top - bottom), 0, 0},
                    {0, 0, 1 / (z_near - z_far), 0},
                    {-(right + left) / (right - left), -(top + bottom) / (top - bottom), z_near / (z_near - z_far), 1}};
        }

        SGCL_INLINE_HOT constexpr float operator()(int row, int column) const noexcept {
            const vec4& c = columns[column];
            return row == 0 ? c.x : row == 1 ? c.y : row == 2 ? c.z : c.w;
        }

        friend constexpr vec4 operator*(const mat4& m, const vec4& v) noexcept;

        // b first, then a
        friend mat4 operator*(const mat4& a, const mat4& b) noexcept;

        SGCL_INLINE_HOT mat4& operator*=(const mat4& b) noexcept {
            return *this = *this * b;
        }

        // A point: w = 1, and the result divided by its w (a projection's
        // perspective divide)
        SGCL_INLINE_HOT constexpr vec3 transform_point(const vec3& p) const noexcept {
            vec4 r = *this * vec4(p, 1);
            return r.w == 1 ? r.xyz() : r.xyz() / r.w;
        }

        // A direction: w = 0, no translation
        SGCL_INLINE_HOT constexpr vec3 transform_vector(const vec3& v) const noexcept {
            return (*this * vec4(v, 0)).xyz();
        }

        // Every vector of the slice replaced by the matrix times it: the
        // vertices of a mesh at once
        void apply(const slice<vec4>& vectors) const noexcept;

        SGCL_INLINE_HOT constexpr mat4 transposed() const noexcept {
            const vec4* c = columns;
            return {{c[0].x, c[1].x, c[2].x, c[3].x},
                    {c[0].y, c[1].y, c[2].y, c[3].y},
                    {c[0].z, c[1].z, c[2].z, c[3].z},
                    {c[0].w, c[1].w, c[2].w, c[3].w}};
        }

        float determinant() const noexcept;

        // The inverse, from the cofactors (Laplace over the 2×2 minors of
        // the top and the bottom two rows); nothing for a determinant of zero
        optional<mat4> inverse() const noexcept;

        // Sixteen floats, column after column, as a GPU takes them
        SGCL_INLINE_HOT const float* data() const noexcept {
            return &columns[0].x;
        }

        friend constexpr bool operator==(const mat4&, const mat4&) noexcept = default;
    };

    namespace detail {
        // a * b on the plain arithmetic: a column of the product is the
        // columns of a weighted by a column of b
        SGCL_INLINE_HOT constexpr mat4 multiply_plain(const mat4& a, const mat4& b) noexcept {
            mat4 r;
            for (int j = 0; j < 4; ++j) {
                r.columns[j] = a * b.columns[j];
            }
            return r;
        }

        SGCL_INLINE_HOT void apply_plain(const mat4& m, vec4* v, size_t n) noexcept {
            for (size_t i = 0; i < n; ++i) {
                v[i] = m * v[i];
            }
        }

#if defined(SGCL_MATH_NEON)
        // The matrix of columns a0..a3 times the vector c by NEON's
        // multiply(-add) by a lane, as two chains summed at the end rather
        // than one of four: a product waited on by the next (a walk down a
        // scene graph, a small batch applied again) finishes sooner
        SGCL_INLINE_HOT float32x4_t column_neon(float32x4_t a0, float32x4_t a1, float32x4_t a2, float32x4_t a3,
                                                float32x4_t c) noexcept {
            float32x4_t s = vmulq_laneq_f32(a0, c, 0);
            float32x4_t t = vmulq_laneq_f32(a1, c, 1);
            s = vfmaq_laneq_f32(s, a2, c, 2);
            t = vfmaq_laneq_f32(t, a3, c, 3);
            return vaddq_f32(s, t);
        }

        // The same by NEON: a column of the product is a's columns weighted
        // by the lanes of b's
        SGCL_INLINE_HOT mat4 multiply_simd(const mat4& a, const mat4& b) noexcept {
            const float* pa = a.data();
            const float* pb = b.data();
            float32x4_t a0 = vld1q_f32(pa);
            float32x4_t a1 = vld1q_f32(pa + 4);
            float32x4_t a2 = vld1q_f32(pa + 8);
            float32x4_t a3 = vld1q_f32(pa + 12);
            mat4 r;
            float* pr = &r.columns[0].x;
            for (int j = 0; j < 4; ++j) {
                vst1q_f32(pr + 4 * j, column_neon(a0, a1, a2, a3, vld1q_f32(pb + 4 * j)));
            }
            return r;
        }

        SGCL_INLINE_HOT void apply_simd(const mat4& m, vec4* v, size_t n) noexcept {
            const float* pm = m.data();
            float32x4_t a0 = vld1q_f32(pm);
            float32x4_t a1 = vld1q_f32(pm + 4);
            float32x4_t a2 = vld1q_f32(pm + 8);
            float32x4_t a3 = vld1q_f32(pm + 12);
            auto* p = &v[0].x;
            for (size_t i = 0; i < n; ++i, p += 4) {
                vst1q_f32(p, column_neon(a0, a1, a2, a3, vld1q_f32(p)));
            }
        }
#elif defined(SGCL_MATH_SSE2)
        // SSE2 has no multiply-add: a product and a sum, each lane of b's
        // column broadcast by a shuffle
        SGCL_INLINE_HOT __m128 column_sse2(__m128 a0, __m128 a1, __m128 a2, __m128 a3, __m128 c) noexcept {
            __m128 s = _mm_add_ps(_mm_mul_ps(a0, _mm_shuffle_ps(c, c, 0x00)), _mm_mul_ps(a1, _mm_shuffle_ps(c, c, 0x55)));
            __m128 t = _mm_add_ps(_mm_mul_ps(a2, _mm_shuffle_ps(c, c, 0xaa)), _mm_mul_ps(a3, _mm_shuffle_ps(c, c, 0xff)));
            return _mm_add_ps(s, t);
        }

        SGCL_INLINE_HOT mat4 multiply_simd(const mat4& a, const mat4& b) noexcept {
            const float* pa = a.data();
            const float* pb = b.data();
            __m128 a0 = _mm_loadu_ps(pa);
            __m128 a1 = _mm_loadu_ps(pa + 4);
            __m128 a2 = _mm_loadu_ps(pa + 8);
            __m128 a3 = _mm_loadu_ps(pa + 12);
            mat4 r;
            float* pr = &r.columns[0].x;
            for (int j = 0; j < 4; ++j) {
                _mm_storeu_ps(pr + 4 * j, column_sse2(a0, a1, a2, a3, _mm_loadu_ps(pb + 4 * j)));
            }
            return r;
        }

        SGCL_INLINE_HOT void apply_simd(const mat4& m, vec4* v, size_t n) noexcept {
            const float* pm = m.data();
            __m128 a0 = _mm_loadu_ps(pm);
            __m128 a1 = _mm_loadu_ps(pm + 4);
            __m128 a2 = _mm_loadu_ps(pm + 8);
            __m128 a3 = _mm_loadu_ps(pm + 12);
            auto* p = &v[0].x;
            for (size_t i = 0; i < n; ++i, p += 4) {
                _mm_storeu_ps(p, column_sse2(a0, a1, a2, a3, _mm_loadu_ps(p)));
            }
        }
#endif
    }

    SGCL_INLINE_HOT constexpr vec4 operator*(const mat4& m, const vec4& v) noexcept {
        return m.columns[0] * v.x + m.columns[1] * v.y + m.columns[2] * v.z + m.columns[3] * v.w;
    }

    SGCL_INLINE_HOT mat4 operator*(const mat4& a, const mat4& b) noexcept {
#if defined(SGCL_MATH_NEON) || defined(SGCL_MATH_SSE2)
        return detail::multiply_simd(a, b);
#else
        return detail::multiply_plain(a, b);
#endif
    }

    SGCL_INLINE_HOT void mat4::apply(const slice<vec4>& vectors) const noexcept {
#if defined(SGCL_MATH_NEON) || defined(SGCL_MATH_SSE2)
        detail::apply_simd(*this, vectors.data(), vectors.size());
#else
        detail::apply_plain(*this, vectors.data(), vectors.size());
#endif
    }

    inline mat4 mat4::rotation(const vec3& axis, float radians) noexcept {
        vec3 n = axis.normalized();
        if (n.length_squared() == 0) {
            return {};
        }
        float s = std::sin(radians);
        float c = std::cos(radians);
        float t = 1 - c;
        // Rodrigues: c·I + s·[n]× + t·n nᵀ
        return {{t * n.x * n.x + c, t * n.x * n.y + s * n.z, t * n.x * n.z - s * n.y, 0},
                {t * n.x * n.y - s * n.z, t * n.y * n.y + c, t * n.y * n.z + s * n.x, 0},
                {t * n.x * n.z + s * n.y, t * n.y * n.z - s * n.x, t * n.z * n.z + c, 0},
                {0, 0, 0, 1}};
    }

    inline mat4 mat4::look_at(const vec3& eye, const vec3& target, const vec3& up) noexcept {
        vec3 f = (target - eye).normalized();   // forward, the camera's -z
        vec3 s = f.cross(up).normalized();      // right, its x
        vec3 u = s.cross(f);                    // its y
        return {{s.x, u.x, -f.x, 0}, {s.y, u.y, -f.y, 0}, {s.z, u.z, -f.z, 0}, {-s.dot(eye), -u.dot(eye), f.dot(eye), 1}};
    }

    inline mat4 mat4::perspective(float fov_y, float aspect, float z_near, float z_far) noexcept {
        float y = 1 / std::tan(fov_y / 2);
        float x = y / aspect;
        // z_clip / w_clip from 0 at -z_near to 1 at -z_far, w_clip = -z_eye
        float a = std::isinf(z_far) ? -1 : z_far / (z_near - z_far);
        float b = std::isinf(z_far) ? -z_near : z_near * z_far / (z_near - z_far);
        return {{x, 0, 0, 0}, {0, y, 0, 0}, {0, 0, a, -1}, {0, 0, b, 0}};
    }

    // The 2×2 minors of the top two rows (s) and of the bottom two (c),
    // their products summed: Laplace's expansion over the two pairs of rows
    inline float mat4::determinant() const noexcept {
        const mat4& m = *this;
        auto e = [&](int r, int c) {
            return double(m(r, c));
        };
        double s0 = e(0, 0) * e(1, 1) - e(1, 0) * e(0, 1);
        double s1 = e(0, 0) * e(1, 2) - e(1, 0) * e(0, 2);
        double s2 = e(0, 0) * e(1, 3) - e(1, 0) * e(0, 3);
        double s3 = e(0, 1) * e(1, 2) - e(1, 1) * e(0, 2);
        double s4 = e(0, 1) * e(1, 3) - e(1, 1) * e(0, 3);
        double s5 = e(0, 2) * e(1, 3) - e(1, 2) * e(0, 3);
        double c5 = e(2, 2) * e(3, 3) - e(3, 2) * e(2, 3);
        double c4 = e(2, 1) * e(3, 3) - e(3, 1) * e(2, 3);
        double c3 = e(2, 1) * e(3, 2) - e(3, 1) * e(2, 2);
        double c2 = e(2, 0) * e(3, 3) - e(3, 0) * e(2, 3);
        double c1 = e(2, 0) * e(3, 2) - e(3, 0) * e(2, 2);
        double c0 = e(2, 0) * e(3, 1) - e(3, 0) * e(2, 1);
        return float(s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0);
    }

    inline optional<mat4> mat4::inverse() const noexcept {
        const mat4& m = *this;
        auto e = [&](int r, int c) {
            return double(m(r, c));
        };
        double s0 = e(0, 0) * e(1, 1) - e(1, 0) * e(0, 1);
        double s1 = e(0, 0) * e(1, 2) - e(1, 0) * e(0, 2);
        double s2 = e(0, 0) * e(1, 3) - e(1, 0) * e(0, 3);
        double s3 = e(0, 1) * e(1, 2) - e(1, 1) * e(0, 2);
        double s4 = e(0, 1) * e(1, 3) - e(1, 1) * e(0, 3);
        double s5 = e(0, 2) * e(1, 3) - e(1, 2) * e(0, 3);
        double c5 = e(2, 2) * e(3, 3) - e(3, 2) * e(2, 3);
        double c4 = e(2, 1) * e(3, 3) - e(3, 1) * e(2, 3);
        double c3 = e(2, 1) * e(3, 2) - e(3, 1) * e(2, 2);
        double c2 = e(2, 0) * e(3, 3) - e(3, 0) * e(2, 3);
        double c1 = e(2, 0) * e(3, 2) - e(3, 0) * e(2, 2);
        double c0 = e(2, 0) * e(3, 1) - e(3, 0) * e(2, 1);
        double det = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
        if (det == 0 || !std::isfinite(det)) {
            return nullopt;
        }
        double k = 1 / det;
        // the adjugate's entries (row, column) over the determinant
        double r[4][4] = {
            {(e(1, 1) * c5 - e(1, 2) * c4 + e(1, 3) * c3) * k, (-e(0, 1) * c5 + e(0, 2) * c4 - e(0, 3) * c3) * k,
             (e(3, 1) * s5 - e(3, 2) * s4 + e(3, 3) * s3) * k, (-e(2, 1) * s5 + e(2, 2) * s4 - e(2, 3) * s3) * k},
            {(-e(1, 0) * c5 + e(1, 2) * c2 - e(1, 3) * c1) * k, (e(0, 0) * c5 - e(0, 2) * c2 + e(0, 3) * c1) * k,
             (-e(3, 0) * s5 + e(3, 2) * s2 - e(3, 3) * s1) * k, (e(2, 0) * s5 - e(2, 2) * s2 + e(2, 3) * s1) * k},
            {(e(1, 0) * c4 - e(1, 1) * c2 + e(1, 3) * c0) * k, (-e(0, 0) * c4 + e(0, 1) * c2 - e(0, 3) * c0) * k,
             (e(3, 0) * s4 - e(3, 1) * s2 + e(3, 3) * s0) * k, (-e(2, 0) * s4 + e(2, 1) * s2 - e(2, 3) * s0) * k},
            {(-e(1, 0) * c3 + e(1, 1) * c1 - e(1, 2) * c0) * k, (e(0, 0) * c3 - e(0, 1) * c1 + e(0, 2) * c0) * k,
             (-e(3, 0) * s3 + e(3, 1) * s1 - e(3, 2) * s0) * k, (e(2, 0) * s3 - e(2, 1) * s1 + e(2, 2) * s0) * k}};
        mat4 inv;
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                (&inv.columns[col].x)[row] = float(r[row][col]);
            }
        }
        return inv;
    }

    // A rotation of 3D space as a unit quaternion: x, y, z the vector part,
    // w the scalar, the identity by default. q * r turns by r, then by q.
    struct quaternion {
        float x = 0;
        float y = 0;
        float z = 0;
        float w = 1;

        constexpr quaternion() noexcept = default;

        SGCL_INLINE_HOT constexpr quaternion(float x, float y, float z, float w) noexcept
        : x(x)
        , y(y)
        , z(z)
        , w(w) {
        }

        SGCL_INLINE_HOT static constexpr quaternion identity() noexcept {
            return {};
        }

        // The turn by the angle about the axis (normalized here; a zero axis
        // is the identity), counter-clockwise looking from its tip, as
        // mat4::rotation
        SGCL_INLINE_HOT static quaternion from_axis_angle(const vec3& axis, float radians) noexcept {
            vec3 n = axis.normalized();
            if (n.length_squared() == 0) {
                return {};
            }
            float s = std::sin(radians / 2);
            return {n.x * s, n.y * s, n.z * s, std::cos(radians / 2)};
        }

        // r first, then l (Hamilton's product)
        SGCL_INLINE_HOT friend constexpr quaternion operator*(const quaternion& l, const quaternion& r) noexcept {
            return {l.w * r.x + l.x * r.w + l.y * r.z - l.z * r.y, l.w * r.y - l.x * r.z + l.y * r.w + l.z * r.x,
                    l.w * r.z + l.x * r.y - l.y * r.x + l.z * r.w, l.w * r.w - l.x * r.x - l.y * r.y - l.z * r.z};
        }

        SGCL_INLINE_HOT constexpr quaternion& operator*=(const quaternion& r) noexcept {
            return *this = *this * r;
        }

        SGCL_INLINE_HOT constexpr quaternion conjugate() const noexcept {
            return {-x, -y, -z, w};
        }

        SGCL_INLINE_HOT constexpr float dot(const quaternion& q) const noexcept {
            return x * q.x + y * q.y + z * q.z + w * q.w;
        }

        SGCL_INLINE_HOT float length() const noexcept {
            return std::sqrt(dot(*this));
        }

        // Of length one; a zero quaternion is the identity
        SGCL_INLINE_HOT quaternion normalized() const noexcept {
            float l = length();
            return l > 0 ? quaternion(x / l, y / l, z / l, w / l) : quaternion();
        }

        // The turn back: the conjugate over the squared length (the
        // conjugate itself for a unit quaternion); a zero one is the identity
        SGCL_INLINE_HOT quaternion inverse() const noexcept {
            float n = dot(*this);
            return n > 0 ? quaternion(-x / n, -y / n, -z / n, w / n) : quaternion();
        }

        // The vector turned: v + 2w(u × v) + 2u × (u × v), u the vector part
        SGCL_INLINE_HOT constexpr vec3 rotate(const vec3& v) const noexcept {
            vec3 u(x, y, z);
            vec3 t = u.cross(v) * 2;
            return v + t * w + u.cross(t);
        }

        // The turn a fraction t of the way from this one to `to` along the
        // shorter arc, at an even pace (spherical linear interpolation); a
        // normalized linear blend when the two are within a hair, where the
        // arc's sine would divide by almost nothing
        quaternion slerp(const quaternion& to, float t) const noexcept {
            float cosine = dot(to);
            quaternion end = to;
            if (cosine < 0) {
                cosine = -cosine;
                end = {-to.x, -to.y, -to.z, -to.w};
            }
            float a;
            float b;
            if (cosine > 0.9995f) {
                a = 1 - t;
                b = t;
                quaternion q(a * x + b * end.x, a * y + b * end.y, a * z + b * end.z, a * w + b * end.w);
                return q.normalized();
            }
            float angle = std::acos(cosine);
            float s = std::sin(angle);
            a = std::sin((1 - t) * angle) / s;
            b = std::sin(t * angle) / s;
            return {a * x + b * end.x, a * y + b * end.y, a * z + b * end.z, a * w + b * end.w};
        }

        // The rotation matrix of a unit quaternion
        SGCL_INLINE_HOT constexpr mat3 to_mat3() const noexcept {
            float xx = x * x, yy = y * y, zz = z * z;
            float xy = x * y, xz = x * z, yz = y * z;
            float wx = w * x, wy = w * y, wz = w * z;
            return {{1 - 2 * (yy + zz), 2 * (xy + wz), 2 * (xz - wy)},
                    {2 * (xy - wz), 1 - 2 * (xx + zz), 2 * (yz + wx)},
                    {2 * (xz + wy), 2 * (yz - wx), 1 - 2 * (xx + yy)}};
        }

        SGCL_INLINE_HOT constexpr mat4 to_mat4() const noexcept {
            mat3 r = to_mat3();
            return {{r.columns[0], 0}, {r.columns[1], 0}, {r.columns[2], 0}, {0, 0, 0, 1}};
        }

        friend constexpr bool operator==(const quaternion&, const quaternion&) noexcept = default;
    };
}

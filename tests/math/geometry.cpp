//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The plane's shapes and transforms (geometry.h) and the small algebra
// (algebra.h) against Python (algebra_vectors.h, from tools/math_vectors.py
// algebra): products, determinants and inverses of matrices whose inverses
// Python computed exactly in fractions, rotations by Rodrigues' formula and
// as quaternions, cameras and projections from their definitions, slerp
// by the angle, the affine transforms of SVG; within float's rounding. Then
// the edges no oracle needs: half-open rectangles, empty ones, the limits of
// int32_t, singular matrices, zero vectors, NaN, and the NEON (or SSE2)
// products against the plain ones.
#include "tests/types.h"
#include "sgcl/math/math.h"
#include "algebra_vectors.h"

#include <cmath>
#include <limits>
#include <random>

using math::affine;
using math::int_point;
using math::int_rect;
using math::int_size;
using math::mat3;
using math::mat4;
using math::point;
using math::quaternion;
using math::rect;
using math::size;
using math::vec2;
using math::vec3;
using math::vec4;

namespace {
    constexpr float Pi = 3.14159265358979f;

    // Within float's rounding of a computation of a few steps, relative to
    // the larger of the two and to the scale of the inputs
    ::testing::AssertionResult near(double got, double want, double scale = 1, double tolerance = 2e-5) {
        double bound = tolerance * std::max({std::fabs(got), std::fabs(want), scale});
        if (std::fabs(got - want) <= bound) {
            return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure() << got << " is not " << want << " within " << bound;
    }

    template<int N>
    auto from_rows(const float* rows) {
        if constexpr (N == 3) {
            mat3 m;
            for (int c = 0; c < 3; ++c) {
                m.columns[c] = vec3(rows[c], rows[3 + c], rows[6 + c]);
            }
            return m;
        } else {
            mat4 m;
            for (int c = 0; c < 4; ++c) {
                m.columns[c] = vec4(rows[c], rows[4 + c], rows[8 + c], rows[12 + c]);
            }
            return m;
        }
    }

    template<int N, class M>
    ::testing::AssertionResult same(const M& m, const float* rows, double scale, double tolerance = 2e-5) {
        for (int r = 0; r < N; ++r) {
            for (int c = 0; c < N; ++c) {
                auto ok = near(m(r, c), rows[r * N + c], scale, tolerance);
                if (!ok) {
                    return ok << " at (" << r << ", " << c << ")";
                }
            }
        }
        return ::testing::AssertionSuccess();
    }

    float largest(const float* p, int n) {
        float m = 0;
        for (int i = 0; i < n; ++i) {
            m = std::max(m, std::fabs(p[i]));
        }
        return m;
    }
}

TEST(Geometry_Tests, MatricesAgainstPython) {
    for (auto& t : algebra_vectors::mat3) {
        mat3 a = from_rows<3>(t.a);
        mat3 b = from_rows<3>(t.b);
        EXPECT_TRUE(same<3>(a * b, t.product, largest(t.a, 9) * largest(t.b, 9)));
        EXPECT_TRUE(near(a.determinant(), t.det, std::pow(largest(t.a, 9), 3.0f)));
        auto inv = a.inverse();
        ASSERT_EQ(inv.has_value(), t.invertible);
        if (inv) {
            EXPECT_TRUE(same<3>(*inv, t.inverse, largest(t.inverse, 9), 1e-4));
        }
        EXPECT_TRUE(same<3>(a.transposed().transposed(), t.a, 1));
        EXPECT_EQ(a.transposed()(0, 1), a(1, 0));
    }
    for (auto& t : algebra_vectors::mat4) {
        mat4 a = from_rows<4>(t.a);
        mat4 b = from_rows<4>(t.b);
        EXPECT_TRUE(same<4>(a * b, t.product, largest(t.a, 16) * largest(t.b, 16)));
        EXPECT_TRUE(near(a.determinant(), t.det, std::pow(largest(t.a, 16), 4.0f)));
        auto inv = a.inverse();
        ASSERT_EQ(inv.has_value(), t.invertible);
        if (inv) {
            EXPECT_TRUE(same<4>(*inv, t.inverse, largest(t.inverse, 16), 1e-4));
        }
        EXPECT_EQ(a.transposed()(2, 3), a(3, 2));
    }
}

TEST(Geometry_Tests, RotationsAgainstPython) {
    for (auto& t : algebra_vectors::rotations) {
        vec3 axis(t.axis[0], t.axis[1], t.axis[2]);
        mat4 m = mat4::rotation(axis, t.angle);
        EXPECT_TRUE(same<4>(m, t.matrix, 1));
        quaternion q = quaternion::from_axis_angle(axis, t.angle);
        EXPECT_TRUE(near(q.x, t.quaternion[0]));
        EXPECT_TRUE(near(q.y, t.quaternion[1]));
        EXPECT_TRUE(near(q.z, t.quaternion[2]));
        EXPECT_TRUE(near(q.w, t.quaternion[3]));
        EXPECT_TRUE(same<4>(q.to_mat4(), t.matrix, 1));
        vec3 v(t.v[0], t.v[1], t.v[2]);
        vec3 turned = q.rotate(v);
        vec3 by_matrix = m.transform_vector(v);
        for (int i = 0; i < 3; ++i) {
            EXPECT_TRUE(near((&turned.x)[i], t.turned[i], 8));
            EXPECT_TRUE(near((&by_matrix.x)[i], t.turned[i], 8));
        }
        // the inverse turns back, and the composition is the product of the matrices
        vec3 back = q.inverse().rotate(turned);
        EXPECT_TRUE(near(back.x, v.x, 8) && near(back.y, v.y, 8) && near(back.z, v.z, 8));
        quaternion twice = q * q;
        mat4 squared_rows = (m * m).transposed();   // its data() is the rows of m * m
        EXPECT_TRUE(same<4>(twice.to_mat4(), squared_rows.data(), 1, 1e-4));
    }
}

TEST(Geometry_Tests, CamerasAgainstPython) {
    for (auto& t : algebra_vectors::cameras) {
        vec3 eye(t.eye[0], t.eye[1], t.eye[2]);
        vec3 target(t.target[0], t.target[1], t.target[2]);
        vec3 up(t.up[0], t.up[1], t.up[2]);
        mat4 view = mat4::look_at(eye, target, up);
        EXPECT_TRUE(same<4>(view, t.view, 10, 1e-4));
        mat4 projection = mat4::perspective(t.fov, t.aspect, t.near, t.far);
        EXPECT_TRUE(same<4>(projection, t.projection, 1, 1e-4));
        // the eye at the origin, the target straight ahead down -z
        vec3 at = view.transform_point(eye);
        EXPECT_TRUE(near(at.x, 0, 10, 1e-4) && near(at.y, 0, 10, 1e-4) && near(at.z, 0, 10, 1e-4));
        vec3 ahead = view.transform_point(target);
        EXPECT_TRUE(near(ahead.x, 0, 10, 1e-4) && near(ahead.y, 0, 10, 1e-4));
        EXPECT_LT(ahead.z, 0);
        // depth 0 at the near plane, 1 at the far one
        EXPECT_TRUE(near(projection.transform_point({0, 0, -t.near}).z, 0, 1, 1e-5));
        EXPECT_TRUE(near(projection.transform_point({0, 0, -t.far}).z, 1, 1, 1e-4));
    }
    mat4 infinite = mat4::perspective(1, 1, 0.5f, std::numeric_limits<float>::infinity());
    EXPECT_TRUE(near(infinite.transform_point({0, 0, -0.5f}).z, 0));
    EXPECT_TRUE(near(infinite.transform_point({0, 0, -1e9f}).z, 1, 1, 1e-6));
    mat4 ortho = mat4::orthographic(-2, 6, -1, 3, 1, 11);
    vec3 low = ortho.transform_point({-2, -1, -1});
    vec3 high = ortho.transform_point({6, 3, -11});
    EXPECT_TRUE(near(low.x, -1) && near(low.y, -1) && near(low.z, 0, 1, 1e-6));
    EXPECT_TRUE(near(high.x, 1) && near(high.y, 1) && near(high.z, 1));
    EXPECT_EQ(mat4::translation({1, 2, 3}).transform_point({1, 1, 1}), vec3(2, 3, 4));
    EXPECT_EQ(mat4::translation({1, 2, 3}).transform_vector({1, 1, 1}), vec3(1, 1, 1));
    EXPECT_EQ(mat4::scaling({2, 3, 4}).transform_point({1, 1, 1}), vec3(2, 3, 4));
}

TEST(Geometry_Tests, SlerpAgainstPython) {
    for (auto& t : algebra_vectors::slerps) {
        quaternion a(t.a[0], t.a[1], t.a[2], t.a[3]);
        quaternion b(t.b[0], t.b[1], t.b[2], t.b[3]);
        quaternion r = a.slerp(b, t.t);
        EXPECT_TRUE(near(r.x, t.result[0], 1, 1e-4) && near(r.y, t.result[1], 1, 1e-4) && near(r.z, t.result[2], 1, 1e-4)
                    && near(r.w, t.result[3], 1, 1e-4))
            << r.x << " " << r.y << " " << r.z << " " << r.w;
        EXPECT_TRUE(near(r.length(), 1, 1, 1e-5));
        quaternion start = a.slerp(b, 0);
        EXPECT_TRUE(near(start.x, a.x) && near(start.w, a.w));
        quaternion end = a.slerp(b, 1);
        EXPECT_TRUE(near(std::fabs(end.dot(b)), 1, 1, 1e-5));
    }
    // two nearly equal: the blend, normalized
    quaternion a = quaternion::from_axis_angle({0, 1, 0}, 0.3f);
    quaternion b = quaternion::from_axis_angle({0, 1, 0}, 0.3001f);
    quaternion m = a.slerp(b, 0.5f);
    EXPECT_TRUE(near(m.length(), 1, 1, 1e-6));
    EXPECT_TRUE(near(m.rotate({1, 0, 0}).x, std::cos(0.30005f), 1, 1e-5));
    // the identity to itself
    EXPECT_EQ(quaternion().slerp(quaternion(), 0.25f), quaternion());
}

TEST(Geometry_Tests, AffineAgainstPython) {
    for (auto& t : algebra_vectors::affines) {
        affine l{t.l[0], t.l[1], t.l[2], t.l[3], t.l[4], t.l[5]};
        affine r{t.r[0], t.r[1], t.r[2], t.r[3], t.r[4], t.r[5]};
        affine p = l * r;
        const float* got = &p.a;
        for (int i = 0; i < 6; ++i) {
            EXPECT_TRUE(near(got[i], t.product[i], 64));
        }
        auto inv = l.inverse();
        ASSERT_TRUE(inv.has_value());
        const float* g = &inv->a;
        for (int i = 0; i < 6; ++i) {
            EXPECT_TRUE(near(g[i], t.inverse[i], largest(t.inverse, 6), 1e-4));
        }
        rect box = l.apply(rect(t.rect[0], t.rect[1], t.rect[2], t.rect[3]));
        EXPECT_TRUE(near(box.x, t.box[0], 64) && near(box.y, t.box[1], 64) && near(box.width, t.box[2], 64)
                    && near(box.height, t.box[3], 64));
        // l then r applied to a point, and the matrix form
        point q(1.5f, -2.25f);
        point lr = p.apply(q);
        point step = l.apply(r.apply(q));
        EXPECT_TRUE(near(lr.x, step.x, 64) && near(lr.y, step.y, 64));
        vec3 by_matrix = mat3(l) * vec3(q.x, q.y, 1);
        point direct = l.apply(q);
        EXPECT_TRUE(near(by_matrix.x, direct.x, 64) && near(by_matrix.y, direct.y, 64) && by_matrix.z == 1);
        EXPECT_TRUE(near(mat3(l).determinant(), l.determinant(), 64));
    }
}

TEST(Geometry_Tests, AffineTransforms) {
    point p = affine::rotation(Pi / 2).apply({1, 0});
    EXPECT_TRUE(near(p.x, 0, 1, 1e-6) && near(p.y, 1));
    point c = affine::rotation(Pi, {10, 10}).apply({11, 10});
    EXPECT_TRUE(near(c.x, 9) && near(c.y, 10));
    EXPECT_EQ(affine::translation(3, 4).apply({1, 1}), point(4, 5));
    EXPECT_EQ(affine::translation(3, 4).apply_vector({1, 1}), point(1, 1));
    EXPECT_EQ(affine::scaling(2, 3).apply({1, 1}), point(2, 3));
    point sk = affine::skew_x(Pi / 4).apply({0, 2});
    EXPECT_TRUE(near(sk.x, 2) && sk.y == 2);
    point sy = affine::skew_y(Pi / 4).apply({2, 0});
    EXPECT_TRUE(sy.x == 2 && near(sy.y, 2));
    // SVG's translate(100, 50) rotate(90): the rotation first
    affine t = affine::translation(100, 50) * affine::rotation(Pi / 2);
    point q = t.apply({10, 0});
    EXPECT_TRUE(near(q.x, 100) && near(q.y, 60));
    rect box = t.apply(rect(0, 0, 20, 10));
    EXPECT_TRUE(near(box.x, 90) && near(box.y, 50) && near(box.width, 10) && near(box.height, 20));
    affine u = affine::identity();
    u *= affine::translation(1, 0);
    u *= affine::scaling(2, 2);
    EXPECT_EQ(u.apply({1, 1}), point(3, 2));
    EXPECT_TRUE(affine().is_identity());
    EXPECT_FALSE(u.is_identity());
    // singular: everything onto a line
    EXPECT_FALSE(affine::scaling(0, 1).inverse().has_value());
    EXPECT_FALSE((affine{1, 2, 2, 4, 5, 6}).inverse().has_value());
    EXPECT_FALSE((affine{std::nanf(""), 0, 0, 1, 0, 0}).inverse().has_value());
    auto inv = t.inverse();
    ASSERT_TRUE(inv.has_value());
    point back = inv->apply(q);
    EXPECT_TRUE(near(back.x, 10, 100) && near(back.y, 0, 100));
}

TEST(Geometry_Tests, Rectangles) {
    rect r(10, 20, 30, 40);
    EXPECT_EQ(r.right(), 40);
    EXPECT_EQ(r.bottom(), 60);
    EXPECT_EQ(r.center(), point(25, 40));
    EXPECT_EQ(r.origin(), point(10, 20));
    EXPECT_EQ(r.extent(), size(30, 40));
    EXPECT_EQ(rect(point(10, 20), size(30, 40)), r);
    EXPECT_EQ(rect::from_points({40, 60}, {10, 20}), r);
    EXPECT_EQ(rect::from_points({40, 20}, {10, 60}), r);
    // half-open: the left and top edges in, the right and bottom out
    EXPECT_TRUE(r.contains(point(10, 20)));
    EXPECT_FALSE(r.contains(point(40, 30)));
    EXPECT_FALSE(r.contains(point(20, 60)));
    EXPECT_TRUE(r.contains(point(39.99f, 59.99f)));
    EXPECT_FALSE(r.contains(point(std::nanf(""), 30)));
    // tiles meeting at an edge do not intersect
    rect right(40, 20, 10, 40);
    EXPECT_FALSE(r.intersects(right));
    EXPECT_TRUE(r.intersection(right).is_empty());
    EXPECT_EQ(r.intersection(rect(30, 50, 100, 100)), rect(30, 50, 10, 10));
    EXPECT_EQ(r.united(right), rect(10, 20, 40, 40));
    // empty ones: no area, inside nothing, ignored by united
    rect empty(5, 5, 0, 10);
    EXPECT_TRUE(empty.is_empty());
    EXPECT_TRUE(rect().is_empty());
    EXPECT_TRUE(rect(0, 0, -1, 5).is_empty());
    EXPECT_FALSE(empty.contains(point(5, 5)));
    EXPECT_FALSE(r.contains(rect(15, 25, 0, 0)));
    EXPECT_FALSE(empty.intersects(r));
    EXPECT_EQ(r.united(empty), r);
    EXPECT_EQ(empty.united(r), r);
    EXPECT_TRUE(r.contains(rect(10, 20, 30, 40)));
    EXPECT_TRUE(r.contains(rect(11, 21, 5, 5)));
    EXPECT_FALSE(r.contains(rect(11, 21, 50, 5)));
    EXPECT_EQ(r.translated(1, -1), rect(11, 19, 30, 40));
    EXPECT_EQ(r.inflated(2, 3), rect(8, 17, 34, 46));
    EXPECT_TRUE(r.inflated(-15, 0).is_empty());
    // rounded outwards to whole pixels
    EXPECT_EQ(rect(0.5f, -0.5f, 1, 1).rounded_out(), int_rect(0, -1, 2, 2));
    EXPECT_EQ(rect(1, 2, 3, 4).rounded_out(), int_rect(1, 2, 3, 4));
    EXPECT_EQ(rect(-1e12f, -1e12f, 2e12f, 2e12f).rounded_out(), int_rect(INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX));
    EXPECT_EQ(rect(1.25f, 1.25f, 0, 0).rounded_out(), int_rect(1, 1, 1, 1));
    // the int forms, with edges in 64 bits
    int_rect big(INT32_MAX - 10, 0, 100, 10);
    EXPECT_EQ(big.right(), int64_t(INT32_MAX) + 90);
    EXPECT_TRUE(big.contains(int_point(INT32_MAX, 5)));
    EXPECT_TRUE(big.intersects(int_rect(INT32_MAX - 1, 5, 1, 1)));
    int_rect a(0, 0, 10, 10);
    EXPECT_FALSE(a.contains(int_point(10, 0)));
    EXPECT_TRUE(a.contains(int_point(9, 9)));
    EXPECT_EQ(a.intersection(int_rect(5, 5, 10, 10)), int_rect(5, 5, 5, 5));
    EXPECT_EQ(a.united(int_rect(20, 20, 1, 1)), int_rect(0, 0, 21, 21));
    EXPECT_TRUE(a.intersection(int_rect(10, 0, 5, 5)).is_empty());
    EXPECT_EQ(int_rect::from_points({10, 10}, {0, 0}), a);
    EXPECT_EQ(int_rect(int_point(1, 2), int_size(3, 4)).extent(), int_size(3, 4));
    EXPECT_EQ(rect(a), rect(0, 0, 10, 10));
    EXPECT_EQ(point(int_point(3, -4)), point(3, -4));
    EXPECT_EQ(size(int_size(3, 4)), size(3, 4));
    EXPECT_EQ(int_point(1, 2) + int_point(3, 4), int_point(4, 6));
    EXPECT_EQ(-int_point(1, 2), int_point(-1, -2));
    EXPECT_TRUE(int_size(0, 5).is_empty());
    EXPECT_FALSE(size(1, 1).is_empty());
    EXPECT_TRUE(size(std::nanf(""), 1).is_empty());
    // points and sizes
    EXPECT_EQ(point(3, 4).distance(point()), 5);
    EXPECT_EQ(point(1, 2) * 2, point(2, 4));
    EXPECT_EQ(2 * point(1, 2), point(2, 4));
    EXPECT_EQ(point(2, 4) / 2, point(1, 2));
    EXPECT_EQ(size(2, 4) * 0.5f, size(1, 2));
    point m(1, 1);
    m += point(1, 2);
    m -= point(0, 1);
    m *= 3;
    m /= 2;
    EXPECT_EQ(m, point(3, 3));
}

TEST(Geometry_Tests, Vectors) {
    EXPECT_EQ(vec3(1, 0, 0).cross(vec3(0, 1, 0)), vec3(0, 0, 1));
    EXPECT_EQ(vec3(0, 1, 0).cross(vec3(1, 0, 0)), vec3(0, 0, -1));
    EXPECT_EQ(vec2(1, 0).cross(vec2(0, 1)), 1);
    EXPECT_EQ(vec2(3, 4).length(), 5);
    EXPECT_EQ(vec3(2, 3, 6).length(), 7);
    EXPECT_EQ(vec4(1, 1, 1, 1).length(), 2);
    EXPECT_EQ(vec3(1, 2, 3).dot(vec3(4, 5, 6)), 32);
    EXPECT_EQ(vec2().normalized(), vec2());
    EXPECT_EQ(vec3().normalized(), vec3());
    EXPECT_EQ(vec4().normalized(), vec4());
    EXPECT_TRUE(near(vec3(1, 2, 2).normalized().length(), 1));
    EXPECT_EQ(vec3(1, 2, 3) * vec3(2, 2, 2), vec3(2, 4, 6));
    EXPECT_EQ(vec4(vec3(1, 2, 3), 4).xyz(), vec3(1, 2, 3));
    EXPECT_EQ(vec3(vec2(1, 2), 3), vec3(1, 2, 3));
    EXPECT_EQ(vec2(point(1, 2)), vec2(1, 2));
    EXPECT_EQ(point(vec2(3, -4)), point(3, -4));
    static_assert(!std::is_convertible_v<vec2, point> && !std::is_convertible_v<point, vec2>);
    static_assert(std::is_constructible_v<point, vec2> && std::is_constructible_v<vec2, point>);
    EXPECT_EQ(vec2(1, 1).distance(vec2(4, 5)), 5);
    vec4 v(1, 2, 3, 4);
    v += vec4(1, 1, 1, 1);
    v -= vec4(0, 0, 0, 1);
    v *= 2;
    v /= 4;
    EXPECT_EQ(v, vec4(1, 1.5f, 2, 2));
    EXPECT_EQ(-v, vec4(-1, -1.5f, -2, -2));
    EXPECT_TRUE(std::isnan(vec3(std::nanf(""), 0, 0).length()));
}

TEST(Geometry_Tests, MatrixEdges) {
    // defaults are the identities, laid out column after column
    mat4 m;
    EXPECT_EQ(m, mat4::identity());
    for (int i = 0; i < 16; ++i) {
        EXPECT_EQ(m.data()[i], i % 5 == 0 ? 1 : 0);
    }
    EXPECT_EQ(mat3().data()[4], 1);
    EXPECT_EQ(mat4::translation({7, 8, 9}).data()[12], 7);
    EXPECT_EQ(mat4::translation({7, 8, 9})(1, 3), 8);
    // singular ones
    EXPECT_FALSE(mat4::scaling({1, 0, 1}).inverse().has_value());
    EXPECT_FALSE(mat3(vec3(1, 2, 3), vec3(2, 4, 6), vec3(0, 0, 1)).inverse().has_value());
    mat4 nan;
    nan.columns[0].x = std::nanf("");
    EXPECT_FALSE(nan.inverse().has_value());
    EXPECT_TRUE(std::isnan(nan.determinant()));
    // a zero axis turns nothing
    EXPECT_EQ(mat4::rotation({0, 0, 0}, 1), mat4());
    EXPECT_EQ(quaternion::from_axis_angle({0, 0, 0}, 1), quaternion());
    EXPECT_EQ(quaternion(0, 0, 0, 0).normalized(), quaternion());
    EXPECT_EQ(quaternion(0, 0, 0, 0).inverse(), quaternion());
    EXPECT_EQ(quaternion(1, 2, 3, 4).conjugate(), quaternion(-1, -2, -3, 4));
    // the product of the projection and the view as a scene uses them
    mat4 mvp = mat4::perspective(1, 16.0f / 9, 0.1f, 100) * mat4::look_at({0, 0, 5}, {0, 0, 0}, {0, 1, 0});
    vec3 center = mvp.transform_point({0, 0, 0});
    EXPECT_TRUE(near(center.x, 0, 1, 1e-6) && near(center.y, 0, 1, 1e-6) && center.z > 0 && center.z < 1);
    mat4 a = mat4::rotation({1, 2, 3}, 0.7f) * mat4::translation({1, -2, 3});
    mat4 b = a;
    b *= mat4::scaling({2, 2, 2});
    EXPECT_TRUE(near(b.determinant(), 8, 8, 1e-5));
    auto inv = a.inverse();
    ASSERT_TRUE(inv.has_value());
    mat4 one = a * *inv;
    for (int i = 0; i < 16; ++i) {
        EXPECT_TRUE(near(one.data()[i], i % 5 == 0 ? 1 : 0, 1, 1e-5));
    }
    mat3 r3 = quaternion::from_axis_angle({0, 0, 1}, Pi / 2).to_mat3();
    vec3 turned = r3 * vec3(1, 0, 0);
    EXPECT_TRUE(near(turned.x, 0, 1, 1e-6) && near(turned.y, 1));
    mat3 r3b = r3;
    r3b *= r3;
    EXPECT_TRUE(near(r3b.determinant(), 1));
}

// The NEON (SSE2) product and batch against the plain loops: equal within the
// rounding of a fused multiply-add, over random matrices and every length of
// the batch up to a few dozen
TEST(Geometry_Tests, VectorPathsAgainstPlain) {
    std::mt19937 rng(3);
    std::uniform_real_distribution<float> d(-10, 10);
    auto random_mat = [&] {
        mat4 m;
        for (int i = 0; i < 16; ++i) {
            (&m.columns[0].x)[i] = d(rng);
        }
        return m;
    };
    for (int i = 0; i < 2000; ++i) {
        mat4 a = random_mat();
        mat4 b = random_mat();
        mat4 fast = a * b;
        mat4 plain = math::detail::multiply_plain(a, b);
        for (int k = 0; k < 16; ++k) {
            ASSERT_TRUE(near(fast.data()[k], plain.data()[k], 400, 1e-5));
        }
    }
    for (size_t n : {0, 1, 2, 3, 4, 5, 7, 8, 15, 16, 17, 33}) {
        mat4 m = random_mat();
        sgcl::vector<vec4> v(n);
        sgcl::vector<vec4> w(n);
        for (size_t i = 0; i < n; ++i) {
            v[i] = w[i] = vec4(d(rng), d(rng), d(rng), d(rng));
        }
        m.apply(v);
        math::detail::apply_plain(m, w.data(), n);
        for (size_t i = 0; i < n; ++i) {
            ASSERT_TRUE(near(v[i].x, w[i].x, 400, 1e-5) && near(v[i].y, w[i].y, 400, 1e-5)
                        && near(v[i].z, w[i].z, 400, 1e-5) && near(v[i].w, w[i].w, 400, 1e-5));
        }
    }
    // an empty slice and a slice of part of the vectors
    sgcl::vector<vec4> some = {vec4(1, 0, 0, 1), vec4(0, 1, 0, 1), vec4(0, 0, 1, 1)};
    mat4::translation({1, 1, 1}).apply(slice<vec4>(some).subslice(1, 1));
    EXPECT_EQ(some[0], vec4(1, 0, 0, 1));
    EXPECT_EQ(some[1], vec4(1, 2, 1, 1));
    EXPECT_EQ(some[2], vec4(0, 0, 1, 1));
    mat4().apply(slice<vec4>());
}

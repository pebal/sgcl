//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Interpolation, easing and Bézier curves (interpolation.h, bezier.h)
// against Python (curves_vectors.h, from tools/math_vectors.py curves): the
// named easings by easings.net's equations, CSS's cubic-bezier() inverted
// by bisection, steps() by the CSS algorithm, the curves by Bernstein's
// polynomials with bounds by dense sampling and lengths by a thousand
// pieces of Gauss-Legendre. Then the properties no table shows: every
// flattened polyline within its tolerance of the curve, the counts of
// Wang's formula, splits that make the same curve, lerp's ends, the edges
// of the tolerance and of the progress.
#include "tests/types.h"
#include "sgcl/math/math.h"
#include "curves_vectors.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <random>

using math::cubic_bezier;
using math::easing;
using math::point;
using math::quadratic_bezier;
using math::rect;
using math::step_position;

namespace {
    ::testing::AssertionResult near(double got, double want, double tolerance) {
        if (std::fabs(got - want) <= tolerance) {
            return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure() << got << " is not " << want << " within " << tolerance;
    }

    const easing* named(const char* name) {
        struct Entry {
            const char* name;
            const easing* e;
        };
        static const Entry entries[] = {
            {"ease_in_sine", &easing::ease_in_sine},         {"ease_out_sine", &easing::ease_out_sine},
            {"ease_in_out_sine", &easing::ease_in_out_sine}, {"ease_in_quad", &easing::ease_in_quad},
            {"ease_out_quad", &easing::ease_out_quad},       {"ease_in_out_quad", &easing::ease_in_out_quad},
            {"ease_in_cubic", &easing::ease_in_cubic},       {"ease_out_cubic", &easing::ease_out_cubic},
            {"ease_in_out_cubic", &easing::ease_in_out_cubic}, {"ease_in_quart", &easing::ease_in_quart},
            {"ease_out_quart", &easing::ease_out_quart},     {"ease_in_out_quart", &easing::ease_in_out_quart},
            {"ease_in_quint", &easing::ease_in_quint},       {"ease_out_quint", &easing::ease_out_quint},
            {"ease_in_out_quint", &easing::ease_in_out_quint}, {"ease_in_expo", &easing::ease_in_expo},
            {"ease_out_expo", &easing::ease_out_expo},       {"ease_in_out_expo", &easing::ease_in_out_expo},
            {"ease_in_circ", &easing::ease_in_circ},         {"ease_out_circ", &easing::ease_out_circ},
            {"ease_in_out_circ", &easing::ease_in_out_circ}, {"ease_in_back", &easing::ease_in_back},
            {"ease_out_back", &easing::ease_out_back},       {"ease_in_out_back", &easing::ease_in_out_back},
            {"ease_in_elastic", &easing::ease_in_elastic},   {"ease_out_elastic", &easing::ease_out_elastic},
            {"ease_in_out_elastic", &easing::ease_in_out_elastic}, {"ease_in_bounce", &easing::ease_in_bounce},
            {"ease_out_bounce", &easing::ease_out_bounce},   {"ease_in_out_bounce", &easing::ease_in_out_bounce},
        };
        for (auto& entry : entries) {
            if (!std::strcmp(entry.name, name)) {
                return entry.e;
            }
        }
        return nullptr;
    }

    // The largest distance from the curve, sampled finely between each two
    // points of the polyline, to the chord between them
    template<class Curve>
    double deviation(const Curve& c, const sgcl::vector<point>& line) {
        size_t n = line.size() - 1;
        double worst = 0;
        for (size_t i = 0; i < n; ++i) {
            point a = line[i];
            point b = line[i + 1];
            double dx = double(b.x) - a.x;
            double dy = double(b.y) - a.y;
            double len = std::hypot(dx, dy);
            for (int k = 1; k < 16; ++k) {
                point p = c.at(float((double(i) + k / 16.0) / double(n)));
                double d;
                if (len == 0) {
                    d = std::hypot(double(p.x) - a.x, double(p.y) - a.y);
                } else {
                    d = std::fabs((double(p.x) - a.x) * dy - (double(p.y) - a.y) * dx) / len;
                }
                worst = std::max(worst, d);
            }
        }
        return worst;
    }
}

TEST(Curves_Tests, NamedEasingsAgainstPython) {
    for (auto& t : curves_vectors::named) {
        const easing* e = named(t.name);
        ASSERT_NE(e, nullptr) << t.name;
        for (int i = 0; i < curves_vectors::progress_count; ++i) {
            EXPECT_TRUE(near((*e)(curves_vectors::progress[i]), t.values[i], 2e-6))
                << t.name << " at " << curves_vectors::progress[i];
        }
        // the ends are exact
        EXPECT_EQ((*e)(0), 0) << t.name;
        EXPECT_EQ((*e)(1), 1) << t.name;
    }
}

TEST(Curves_Tests, CubicBezierAgainstPython) {
    for (auto& t : curves_vectors::beziers) {
        easing e = easing::bezier(t.p[0], t.p[1], t.p[2], t.p[3]);
        for (int i = 0; i < curves_vectors::progress_count; ++i) {
            EXPECT_TRUE(near(e(curves_vectors::progress[i]), t.values[i], 2e-6))
                << t.p[0] << " " << t.p[1] << " " << t.p[2] << " " << t.p[3] << " at " << curves_vectors::progress[i];
        }
    }
    // CSS's keywords are those curves
    EXPECT_EQ(easing::ease, easing::bezier(0.25f, 0.1f, 0.25f, 1));
    EXPECT_EQ(easing::ease_in, easing::bezier(0.42f, 0, 1, 1));
    EXPECT_EQ(easing::ease_out, easing::bezier(0, 0, 0.58f, 1));
    EXPECT_EQ(easing::ease_in_out, easing::bezier(0.42f, 0, 0.58f, 1));
    EXPECT_TRUE(near(easing::ease(0.5f), 0.8024033877399112, 1e-6));
    // x outside [0, 1] is held to it, as CSS requires
    EXPECT_EQ(easing::bezier(-1, 0, 2, 1), easing::bezier(0, 0, 1, 1));
}

TEST(Curves_Tests, StepsAgainstCss) {
    for (auto& t : curves_vectors::steps) {
        easing e = easing::steps(t.count, step_position(t.position));
        for (int i = 0; i < curves_vectors::progress_count; ++i) {
            EXPECT_EQ(e(curves_vectors::progress[i]), float(t.values[i]))
                << t.count << " " << t.position << " at " << curves_vectors::progress[i];
        }
    }
    // CSS's examples: steps(4, jump-end) 0.25 at 0.3, steps(2, jump-start) 0.5 at 0
    EXPECT_EQ(easing::steps(4)(0.3f), 0.25f);
    EXPECT_EQ(easing::steps(2, step_position::jump_start)(0), 0.5f);
    EXPECT_EQ(easing::steps(3, step_position::jump_none)(1), 1);
    EXPECT_EQ(easing::steps(3, step_position::jump_both)(0), 0.25f);
    // a count below the least is the least
    EXPECT_EQ(easing::steps(0), easing::steps(1));
    EXPECT_EQ(easing::steps(-5, step_position::jump_none), easing::steps(2, step_position::jump_none));
}

TEST(Curves_Tests, EasingEdges) {
    easing linear;
    EXPECT_EQ(linear, easing::linear);
    EXPECT_EQ(linear(0.3f), 0.3f);
    // the progress held to [0, 1]
    EXPECT_EQ(linear(-1), 0);
    EXPECT_EQ(linear(2), 1);
    EXPECT_EQ(easing::ease_out_bounce(5), 1);
    EXPECT_EQ(easing::ease(-0.5f), 0);
    EXPECT_TRUE(std::isnan(easing::ease(std::nanf(""))));
    EXPECT_TRUE(std::isnan(easing::steps(3)(std::nanf(""))));
    // what comes out may leave [0, 1]
    EXPECT_LT(easing::ease_in_back(0.2f), 0);
    EXPECT_GT(easing::ease_out_elastic(0.1f), 1);
    EXPECT_NE(easing::ease_in_quad, easing::ease_out_quad);
    // monotonic curves stay monotonic, finely
    for (const easing* e : {&easing::ease, &easing::ease_in_out, &easing::ease_in_out_cubic, &easing::ease_out_expo}) {
        float last = 0;
        for (int i = 1; i <= 10000; ++i) {
            float v = (*e)(float(i) / 10000);
            ASSERT_GE(v, last - 1e-6f) << i;
            last = v;
        }
    }
}

TEST(Curves_Tests, Lerp) {
    EXPECT_EQ(math::lerp(1.0f, 3.0f, 0.5f), 2.0f);
    EXPECT_EQ(math::lerp(0.1, 0.7, 1.0), 0.7);
    EXPECT_EQ(math::lerp(0.1f, 0.7f, 0.0f), 0.1f);
    EXPECT_EQ(math::lerp(1.0f, 3.0f, 2.0f), 5.0f);
    EXPECT_EQ(math::lerp(point(0, 10), point(10, 0), 0.25f), point(2.5f, 7.5f));
    EXPECT_EQ(math::lerp(math::vec3(0, 0, 0), math::vec3(2, 4, 6), 0.5f), math::vec3(1, 2, 3));
    EXPECT_EQ(math::lerp(math::vec2(0, 0), math::vec2(2, 4), 1.0f), math::vec2(2, 4));
    EXPECT_EQ(math::lerp(math::vec4(1, 1, 1, 1), math::vec4(3, 3, 3, 3), 0.5f), math::vec4(2, 2, 2, 2));
    EXPECT_EQ(math::inverse_lerp(10.0f, 20.0f, 15.0f), 0.5f);
    EXPECT_EQ(math::inverse_lerp(10.0, 20.0, 30.0), 2.0);
    EXPECT_EQ(math::inverse_lerp(5.0f, 5.0f, 7.0f), 0.0f);
    EXPECT_EQ(math::smoothstep(0.0f, 1.0f, 0.5f), 0.5f);
    EXPECT_EQ(math::smoothstep(0.0f, 1.0f, -1.0f), 0.0f);
    EXPECT_EQ(math::smoothstep(0.0f, 1.0f, 2.0f), 1.0f);
    EXPECT_EQ(math::smoothstep(0.0, 2.0, 0.5), 0.15625);
    EXPECT_EQ(math::smoothstep(1.0f, 1.0f, 0.5f), 0.0f);
    EXPECT_EQ(math::smoothstep(1.0f, 1.0f, 1.0f), 1.0f);
    EXPECT_EQ(math::smoothstep(1.0f, 0.0f, 0.25f), 0.84375f);   // edges the other way
    // the ends exact for any pair
    std::mt19937 rng(9);
    std::uniform_real_distribution<float> d(-1e6f, 1e6f);
    for (int i = 0; i < 10000; ++i) {
        float a = d(rng);
        float b = d(rng);
        ASSERT_EQ(math::lerp(a, b, 0.0f), a);
        ASSERT_EQ(math::lerp(a, b, 1.0f), b);
    }
}

TEST(Curves_Tests, CurvesAgainstPython) {
    for (auto& t : curves_vectors::quadratics) {
        quadratic_bezier c{{t.p[0], t.p[1]}, {t.p[2], t.p[3]}, {t.p[4], t.p[5]}};
        double scale = 300;
        point a = c.at(t.t);
        EXPECT_TRUE(near(a.x, t.at[0], scale * 1e-6) && near(a.y, t.at[1], scale * 1e-6));
        point d = c.derivative(t.t);
        EXPECT_TRUE(near(d.x, t.derivative[0], scale * 2e-6) && near(d.y, t.derivative[1], scale * 2e-6));
        rect b = c.bounds();
        EXPECT_TRUE(near(b.x, t.bounds[0], 1e-3) && near(b.y, t.bounds[1], 1e-3) && near(b.width, t.bounds[2], 1e-3)
                    && near(b.height, t.bounds[3], 1e-3))
            << b.x << " " << b.y << " " << b.width << " " << b.height;
        EXPECT_TRUE(near(c.length(), t.length, 2e-3 + t.length * 1e-6));
        // the degree raised is the same curve
        cubic_bezier raised = c.to_cubic();
        for (float u : {0.0f, 0.3f, 0.5f, 0.9f, 1.0f}) {
            EXPECT_TRUE(near(raised.at(u).x, c.at(u).x, 1e-4) && near(raised.at(u).y, c.at(u).y, 1e-4));
        }
        EXPECT_TRUE(near(raised.length(), t.length, 2e-3 + t.length * 1e-6));
    }
    for (auto& t : curves_vectors::cubics) {
        cubic_bezier c{{t.p[0], t.p[1]}, {t.p[2], t.p[3]}, {t.p[4], t.p[5]}, {t.p[6], t.p[7]}};
        double scale = 300;
        point a = c.at(t.t);
        EXPECT_TRUE(near(a.x, t.at[0], scale * 1e-6) && near(a.y, t.at[1], scale * 1e-6));
        point d = c.derivative(t.t);
        EXPECT_TRUE(near(d.x, t.derivative[0], scale * 3e-6) && near(d.y, t.derivative[1], scale * 3e-6));
        rect b = c.bounds();
        EXPECT_TRUE(near(b.x, t.bounds[0], 1e-3) && near(b.y, t.bounds[1], 1e-3) && near(b.width, t.bounds[2], 1e-3)
                    && near(b.height, t.bounds[3], 1e-3))
            << b.x << " " << b.y << " " << b.width << " " << b.height;
        EXPECT_TRUE(near(c.length(), t.length, 2e-3 + t.length * 1e-6));
    }
}

// Every polyline within its tolerance of the curve, for random curves and
// tolerances; the count is Wang's and the ends are the curve's
TEST(Curves_Tests, FlattenWithinTolerance) {
    std::mt19937 rng(21);
    std::uniform_real_distribution<float> d(-500, 500);
    for (int i = 0; i < 400; ++i) {
        float tolerance = std::pow(10.0f, float(rng() % 5) - 3);   // 0.001 … 10
        cubic_bezier c{{d(rng), d(rng)}, {d(rng), d(rng)}, {d(rng), d(rng)}, {d(rng), d(rng)}};
        sgcl::vector<point> line = c.flatten(tolerance);
        ASSERT_EQ(line.size(), c.segments(tolerance) + 1);
        ASSERT_EQ(line[0], c.p0);
        ASSERT_EQ(line[line.size() - 1], c.p3);
        ASSERT_LE(deviation(c, line), tolerance * 1.01 + 1e-4) << i;
        quadratic_bezier q{{d(rng), d(rng)}, {d(rng), d(rng)}, {d(rng), d(rng)}};
        sgcl::vector<point> qline = q.flatten(tolerance);
        ASSERT_EQ(qline.size(), q.segments(tolerance) + 1);
        ASSERT_LE(deviation(q, qline), tolerance * 1.01 + 1e-4) << i;
        // the flattened length is below the true one, and near it
        double sum = 0;
        for (size_t k = 1; k < line.size(); ++k) {
            sum += std::hypot(double(line[k].x) - line[k - 1].x, double(line[k].y) - line[k - 1].y);
        }
        ASSERT_LE(sum, c.length() * (1 + 1e-5) + 1e-3);
    }
    // a path's segments appended into one polyline
    sgcl::vector<point> path;
    cubic_bezier first{{0, 0}, {0, 50}, {50, 50}, {50, 0}};
    quadratic_bezier second{{50, 0}, {75, -50}, {100, 0}};
    path.push_back(first.p0);
    first.flatten(path, 0.5f);
    second.flatten(path, 0.5f);
    EXPECT_EQ(path.size(), 1 + first.segments(0.5f) + second.segments(0.5f));
    EXPECT_EQ(path[path.size() - 1], point(100, 0));
}

TEST(Curves_Tests, CurveEdges) {
    // a straight line flattens to one segment, a point to one of no length
    cubic_bezier line{{0, 0}, {1, 1}, {2, 2}, {3, 3}};
    EXPECT_EQ(line.segments(0.25f), 1u);
    EXPECT_EQ(line.flatten().size(), 2u);
    cubic_bezier dot{{5, 5}, {5, 5}, {5, 5}, {5, 5}};
    EXPECT_EQ(dot.flatten().size(), 2u);
    EXPECT_EQ(dot.length(), 0);
    EXPECT_EQ(dot.bounds(), rect(5, 5, 0, 0));
    // a tolerance of zero, below or NaN is a ten-thousandth of the extent,
    // never an endless subdivision; a tiny one is held to 2^16 segments
    cubic_bezier arc{{0, 0}, {0, 100}, {100, 100}, {100, 0}};
    size_t least = arc.segments(0.01f);
    EXPECT_EQ(arc.segments(1e-30f), size_t(1) << 16);
    EXPECT_EQ(arc.segments(0), least);
    EXPECT_EQ(arc.segments(-1), least);
    EXPECT_EQ(arc.segments(std::nanf("")), least);
    EXPECT_LE(least, size_t(1) << 16);
    // coordinates near float's limit: held to 2^16 segments
    cubic_bezier huge{{0, 0}, {3e38f, 0}, {-3e38f, 3e38f}, {0, 0}};
    EXPECT_EQ(huge.segments(0.25f), size_t(1) << 16);
    // splits are the same curve
    auto [left, right] = arc.split(0.3f);
    EXPECT_EQ(left.p0, arc.p0);
    EXPECT_EQ(right.p3, arc.p3);
    EXPECT_EQ(left.p3, right.p0);
    for (float u : {0.0f, 0.25f, 0.5f, 1.0f}) {
        point l = left.at(u);
        point want = arc.at(0.3f * u);
        EXPECT_TRUE(near(l.x, want.x, 1e-3) && near(l.y, want.y, 1e-3));
        point r = right.at(u);
        point rw = arc.at(0.3f + 0.7f * u);
        EXPECT_TRUE(near(r.x, rw.x, 1e-3) && near(r.y, rw.y, 1e-3));
    }
    EXPECT_TRUE(near(left.length() + right.length(), arc.length(), 1e-2));
    auto [ql, qr] = quadratic_bezier{{0, 0}, {50, 100}, {100, 0}}.split(0.5f);
    EXPECT_EQ(ql.p2, point(50, 50));
    EXPECT_EQ(qr.p0, point(50, 50));
    // ends and tangents
    EXPECT_EQ(arc.at(0), arc.p0);
    EXPECT_EQ(arc.at(1), arc.p3);
    EXPECT_EQ(arc.derivative(0), point(0, 300));
    EXPECT_EQ(arc.derivative(1), point(0, -300));
    EXPECT_EQ(arc.bounds(), rect(0, 0, 100, 75));
    // a tighter tolerance of the length costs more and agrees
    EXPECT_TRUE(near(arc.length(1e-6f), arc.length(1e-2f), 1e-2));
}

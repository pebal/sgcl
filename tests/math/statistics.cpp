//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The statistics (statistics.h) against Python's statistics module and
// exact fractions (statistics_vectors.h, from tools/math_vectors.py
// statistics): mean, median, quartiles, deciles and percentiles, variance
// and standard deviation of sample and population, skewness and kurtosis,
// mode, covariance, correlation and the least-squares line, over sets of
// one value to a thousand, a large offset and tiny values among them. Then
// what Python has not: merges equal to one accumulator, the t-digest's error
// of rank against the exact quantiles of a million values of three
// distributions and its merge, the histogram's buckets and Prometheus's
// quantile, NaN, empty accumulators and the errors of the program.
#include "tests/types.h"
#include "sgcl/math/math.h"
#include "statistics_vectors.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <vector>

using math::histogram;
using math::paired_summary;
using math::summary;
using math::t_digest;

namespace {
    ::testing::AssertionResult close(double got, double want, double relative, double absolute = 0) {
        if (std::isnan(want) && std::isnan(got)) {
            return ::testing::AssertionSuccess();
        }
        double bound = std::max(absolute, relative * std::fabs(want));
        if (std::fabs(got - want) <= bound) {
            return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure() << got << " is not " << want << " within " << bound;
    }
}

TEST(Statistics_Tests, AgainstPython) {
    for (auto& t : statistics_vectors::sets) {
        std::vector<double> data(t.data, t.data + t.size);
        summary s(data);
        double scale = std::max(std::fabs(t.mean), 1e-300);
        EXPECT_EQ(s.count(), t.size) << t.name;
        EXPECT_TRUE(close(math::mean(data), t.mean, 1e-15, 1e-15 * scale)) << t.name;
        EXPECT_TRUE(close(s.mean(), t.mean, 1e-14, 1e-14 * std::sqrt(t.pvariance))) << t.name;
        EXPECT_EQ(math::median(data), t.median) << t.name;
        EXPECT_TRUE(close(s.variance(), t.variance, 1e-12)) << t.name;
        EXPECT_TRUE(close(s.population_variance(), t.pvariance, 1e-12)) << t.name;
        EXPECT_TRUE(close(s.stddev(), t.stdev, 1e-12)) << t.name;
        EXPECT_TRUE(close(s.population_stddev(), t.pstdev, 1e-12)) << t.name;
        EXPECT_TRUE(close(s.skewness(), t.skewness, 1e-9, 1e-12)) << t.name;
        EXPECT_TRUE(close(s.kurtosis(), t.kurtosis, 1e-9, 1e-12)) << t.name;
        EXPECT_EQ(math::mode(data), t.mode) << t.name;
        EXPECT_EQ(s.min(), *std::min_element(data.begin(), data.end()));
        EXPECT_EQ(s.max(), *std::max_element(data.begin(), data.end()));
        for (int i = 0; i < 3; ++i) {
            EXPECT_TRUE(close(math::quantile(data, (i + 1) / 4.0), t.quartiles[i], 1e-14, 1e-300)) << t.name << " q" << i;
        }
        for (int i = 0; i < 9; ++i) {
            EXPECT_TRUE(close(math::quantile(data, (i + 1) / 10.0), t.deciles[i], 1e-13, 1e-300)) << t.name << " d" << i;
        }
        for (int i = 0; i < 99; ++i) {
            EXPECT_TRUE(close(math::quantile(data, (i + 1) / 100.0), t.percentiles[i], 1e-12, 1e-290)) << t.name << " p" << i;
        }
        // the sum, compensated, against the mean's
        EXPECT_TRUE(close(s.sum() / double(t.size), t.mean, 1e-15, 1e-15 * scale)) << t.name;
    }
    for (auto& t : statistics_vectors::pairs) {
        paired_summary p;
        for (size_t i = 0; i < t.size; ++i) {
            p.add(t.x[i], t.y[i]);
        }
        EXPECT_TRUE(close(p.covariance(), t.covariance, 1e-11)) << t.name;
        EXPECT_TRUE(close(p.correlation(), t.correlation, 1e-11)) << t.name;
        EXPECT_TRUE(close(p.slope(), t.slope, 1e-10)) << t.name;
        EXPECT_TRUE(close(p.intercept(), t.intercept, 1e-9, 1e-9)) << t.name;
        EXPECT_TRUE(close(p.population_covariance(), t.covariance * double(t.size - 1) / double(t.size), 1e-11)) << t.name;
    }
}

// A merge is the accumulator of every value, split anywhere
TEST(Statistics_Tests, MergesAreTheWhole) {
    std::mt19937_64 rng(41);
    std::normal_distribution<double> d(5, 2);
    for (int r = 0; r < 200; ++r) {
        size_t n = rng() % 300 + 2;
        std::vector<double> xs(n);
        std::vector<double> ys(n);
        for (size_t i = 0; i < n; ++i) {
            xs[i] = d(rng) * (r % 7 + 1);
            ys[i] = xs[i] * 0.5 + d(rng);
        }
        size_t cut = rng() % (n + 1);
        summary whole(xs);
        summary a(slice<const double>(xs.data(), cut));
        summary b(slice<const double>(xs.data() + cut, n - cut));
        a.merge(b);
        ASSERT_EQ(a.count(), whole.count());
        ASSERT_TRUE(close(a.mean(), whole.mean(), 1e-13, 1e-13));
        ASSERT_TRUE(close(a.variance(), whole.variance(), 1e-11));
        ASSERT_TRUE(close(a.skewness(), whole.skewness(), 1e-8, 1e-10));
        ASSERT_TRUE(close(a.kurtosis(), whole.kurtosis(), 1e-8, 1e-10));
        ASSERT_EQ(a.min(), whole.min());
        ASSERT_EQ(a.max(), whole.max());
        paired_summary pw;
        paired_summary pa;
        paired_summary pb;
        for (size_t i = 0; i < n; ++i) {
            pw.add(xs[i], ys[i]);
            (i < cut ? pa : pb).add(xs[i], ys[i]);
        }
        pa.merge(pb);
        ASSERT_TRUE(close(pa.covariance(), pw.covariance(), 1e-10, 1e-12));
        ASSERT_TRUE(close(pa.correlation(), pw.correlation(), 1e-10, 1e-12));
        ASSERT_TRUE(close(pa.slope(), pw.slope(), 1e-10, 1e-12));
    }
}

TEST(Statistics_Tests, EmptyAndUndefined) {
    summary s;
    EXPECT_EQ(s.count(), 0u);
    EXPECT_EQ(s.sum(), 0);
    EXPECT_TRUE(std::isnan(s.mean()));
    EXPECT_TRUE(std::isnan(s.variance()));
    EXPECT_TRUE(std::isnan(s.population_variance()));
    EXPECT_TRUE(std::isnan(s.skewness()));
    EXPECT_EQ(s.min(), HUGE_VAL);
    EXPECT_EQ(s.max(), -HUGE_VAL);
    s.add(3);
    EXPECT_EQ(s.mean(), 3);
    EXPECT_TRUE(std::isnan(s.variance()));
    EXPECT_EQ(s.population_variance(), 0);
    EXPECT_TRUE(std::isnan(s.skewness()));   // constant
    summary empty;
    s.merge(empty);
    EXPECT_EQ(s.count(), 1u);
    empty.merge(s);
    EXPECT_EQ(empty.mean(), 3);
    // NaN goes through the moments, never into min or max
    summary n;
    n.add(1);
    n.add(std::nan(""));
    n.add(3);
    EXPECT_TRUE(std::isnan(n.mean()));
    EXPECT_EQ(n.min(), 1);
    EXPECT_EQ(n.max(), 3);
    paired_summary p;
    EXPECT_TRUE(std::isnan(p.mean_x()));
    EXPECT_TRUE(std::isnan(p.covariance()));
    p.add(1, 5);
    p.add(1, 7);
    EXPECT_TRUE(std::isnan(p.correlation()));   // x constant
    EXPECT_TRUE(std::isnan(p.slope()));
    EXPECT_EQ(p.mean_y(), 6);
    EXPECT_EQ(p.covariance(), 0);
    // the functions of a sequence refuse nothing and NaN
    std::vector<double> none;
    std::vector<double> bad = {1, std::nan(""), 2};
    std::vector<double> some = {3, 1, 2};
    EXPECT_THROW(math::mean(none), std::domain_error);
    EXPECT_THROW(math::median(none), std::domain_error);
    EXPECT_THROW(math::quantile(none, 0.5), std::domain_error);
    EXPECT_THROW(math::mode(none), std::domain_error);
    EXPECT_THROW(math::median(bad), std::domain_error);
    EXPECT_THROW(math::quantile(bad, 0.5), std::domain_error);
    EXPECT_THROW(math::mode(bad), std::domain_error);
    EXPECT_THROW(math::quantile(some, -0.1), std::domain_error);
    EXPECT_THROW(math::quantile(some, 1.1), std::domain_error);
    EXPECT_THROW(math::quantile(some, std::nan("")), std::domain_error);
    EXPECT_EQ(math::quantile(some, 0), 1);
    EXPECT_EQ(math::quantile(some, 1), 3);
    EXPECT_EQ(math::median(some), 2);
    EXPECT_TRUE(std::isnan(math::mean(bad)));
    // mode: the first of the most common, -0 and +0 one value
    EXPECT_EQ(math::mode(std::vector<double>{2, 1, 1, 2, 3}), 2);
    EXPECT_EQ(math::mode(std::vector<double>{5}), 5);
    EXPECT_EQ(math::mode(std::vector<double>{-0.0, 1, 0.0}), 0);
    // infinities are values
    std::vector<double> inf = {1, HUGE_VAL, 2};
    EXPECT_EQ(math::median(inf), 2);
    EXPECT_EQ(math::quantile(inf, 1), HUGE_VAL);
    EXPECT_EQ(math::quantile(inf, 0.75), HUGE_VAL);
    std::vector<double> ends = {-HUGE_VAL, 5, 6, HUGE_VAL};
    EXPECT_EQ(math::quantile(ends, 0.1), -HUGE_VAL);
    EXPECT_EQ(math::median(std::vector<double>{-HUGE_VAL, 5}), -HUGE_VAL);
    EXPECT_TRUE(std::isnan(math::median(std::vector<double>{-HUGE_VAL, HUGE_VAL})));
    // the largest numbers of opposite signs: their difference overflows, the answer may not
    double big = std::numeric_limits<double>::max();
    EXPECT_EQ(math::median(std::vector<double>{-big, big}), 0);
    EXPECT_EQ(math::median(std::vector<double>{big, big}), big);
    double q = math::quantile(std::vector<double>{-big, big}, 0.25);
    EXPECT_TRUE(std::isfinite(q) && q < 0);
    EXPECT_EQ(math::quantile(std::vector<double>{big, big}, 0.3), big);
    EXPECT_EQ(math::median(ends), 5.5);
}

// The t-digest's quantiles against the exact ones of a million values: the
// error of rank within about 1/δ in the middle, far less at the tails
TEST(Statistics_Tests, TDigestAccuracy) {
    std::mt19937_64 rng(43);
    double worst[2] = {0, 0};   // the largest error of rank at the tails and in the middle
    constexpr size_t N = 1000000;
    for (int dist = 0; dist < 3; ++dist) {
        std::vector<double> values(N);
        std::normal_distribution<double> normal(0, 1);
        std::exponential_distribution<double> exponential(1);
        std::uniform_real_distribution<double> uniform(-5, 5);
        for (auto& v : values) {
            v = dist == 0 ? normal(rng) : dist == 1 ? exponential(rng) : uniform(rng);
        }
        t_digest digest;
        t_digest a;
        t_digest b;
        for (size_t i = 0; i < N; ++i) {
            digest.add(values[i]);
            (i % 3 ? a : b).add(values[i]);
        }
        a.merge(b);
        std::vector<double> sorted = values;
        std::sort(sorted.begin(), sorted.end());
        EXPECT_EQ(digest.count(), N);
        EXPECT_EQ(digest.min(), sorted.front());
        EXPECT_EQ(digest.max(), sorted.back());
        EXPECT_EQ(digest.quantile(0), sorted.front());
        EXPECT_EQ(digest.quantile(1), sorted.back());
        for (double q : {0.0001, 0.001, 0.01, 0.1, 0.25, 0.5, 0.75, 0.9, 0.99, 0.999, 0.9999}) {
            for (const t_digest* d : {&digest, &a}) {
                double estimate = d->quantile(q);
                // the rank of the estimate among the values
                double rank = double(std::lower_bound(sorted.begin(), sorted.end(), estimate) - sorted.begin()) / N;
                // about 1/δ of the rank at the middle, shrinking as q(1 - q)
                // towards the ends, and a floor at the extreme tails
                double allowed = std::max(0.0005, 4 * q * (1 - q) / 100 * 2);
                worst[q < 0.05 || q > 0.95 ? 0 : 1] = std::max(worst[q < 0.05 || q > 0.95 ? 0 : 1], std::fabs(rank - q));
                EXPECT_LT(std::fabs(rank - q), allowed) << "distribution " << dist << " q " << q;
                EXPECT_TRUE(close(d->cdf(estimate), q, 0, allowed)) << "distribution " << dist << " q " << q;
            }
        }
        // monotonic in q
        double last = -HUGE_VAL;
        for (int i = 0; i <= 1000; ++i) {
            double v = digest.quantile(i / 1000.0);
            ASSERT_GE(v, last);
            last = v;
        }
    }
    RecordProperty("worst_rank_error_tails", std::to_string(worst[0]));
    RecordProperty("worst_rank_error_middle", std::to_string(worst[1]));
    std::printf("t-digest (compression 100, 1e6 values): worst rank error %.5f at the tails, %.5f in the middle\n", worst[0], worst[1]);
}

TEST(Statistics_Tests, TDigestEdges) {
    t_digest d;
    EXPECT_TRUE(std::isnan(d.quantile(0.5)));
    EXPECT_TRUE(std::isnan(d.cdf(1)));
    EXPECT_EQ(d.count(), 0u);
    d.add(std::nan(""));
    EXPECT_EQ(d.count(), 0u);
    d.add(7);
    EXPECT_EQ(d.quantile(0), 7);
    EXPECT_EQ(d.quantile(0.5), 7);
    EXPECT_EQ(d.quantile(1), 7);
    EXPECT_EQ(d.cdf(6), 0);
    EXPECT_EQ(d.cdf(7), 1);
    // a few values: exact, as each is a centroid of its own
    t_digest few;
    for (double x : {1.0, 2.0, 3.0, 4.0, 5.0}) {
        few.add(x);
    }
    EXPECT_EQ(few.quantile(0.5), 3);
    EXPECT_EQ(few.quantile(0), 1);
    EXPECT_EQ(few.quantile(1), 5);
    EXPECT_TRUE(close(few.cdf(3), 0.5, 0, 1e-12));
    EXPECT_THROW(few.quantile(-0.01), std::domain_error);
    EXPECT_THROW(few.quantile(1.01), std::domain_error);
    EXPECT_THROW(t_digest(5), std::invalid_argument);
    EXPECT_THROW(t_digest(std::nan("")), std::invalid_argument);
    EXPECT_EQ(t_digest(200).compression(), 200);
    // infinities counted at the ends, the centroids of the numbers between
    t_digest ends;
    for (double x : {-HUGE_VAL, 1.0, 2.0, 3.0, HUGE_VAL, HUGE_VAL}) {
        ends.add(x);
    }
    EXPECT_EQ(ends.count(), 6u);
    EXPECT_EQ(ends.min(), -HUGE_VAL);
    EXPECT_EQ(ends.max(), HUGE_VAL);
    EXPECT_EQ(ends.quantile(0), -HUGE_VAL);
    EXPECT_EQ(ends.quantile(1), HUGE_VAL);
    EXPECT_EQ(ends.quantile(0.9), HUGE_VAL);
    EXPECT_TRUE(std::isfinite(ends.quantile(0.4)));
    EXPECT_TRUE(close(ends.cdf(2.5), 2.5 / 6, 0, 0.2));
    EXPECT_EQ(ends.cdf(HUGE_VAL), 1);
    EXPECT_EQ(ends.cdf(-HUGE_VAL), 0);
    t_digest only;
    only.add(HUGE_VAL);
    EXPECT_EQ(only.quantile(0.5), HUGE_VAL);
    EXPECT_EQ(only.min(), HUGE_VAL);
    // a copy is a snapshot
    t_digest copy = few;
    few.add(100);
    EXPECT_EQ(copy.max(), 5);
    EXPECT_EQ(few.max(), 100);
    // merging an empty one changes nothing, into an empty one copies
    t_digest empty;
    copy.merge(empty);
    EXPECT_EQ(copy.count(), 5u);
    empty.merge(copy);
    EXPECT_EQ(empty.quantile(0.5), 3);
    // the centroids stay bounded: about 2δ after a million values
    t_digest big(50);
    std::mt19937_64 rng(47);
    for (int i = 0; i < 1000000; ++i) {
        big.add(double(rng() >> 11));
    }
    EXPECT_EQ(big.count(), 1000000u);
    EXPECT_TRUE(std::isfinite(big.quantile(0.5)));
}

TEST(Statistics_Tests, Histogram) {
    histogram h = histogram::exponential(1, 2, 4);   // 1, 2, 4, 8, +inf
    EXPECT_EQ(h.bucket_count(), 5u);
    EXPECT_EQ(h.upper_bound(0), 1);
    EXPECT_EQ(h.upper_bound(3), 8);
    EXPECT_EQ(h.upper_bound(4), HUGE_VAL);
    for (double x : {0.5, 1.0, 1.5, 2.0, 3.0, 8.0, 9.0, -5.0, std::nan("")}) {
        h.add(x);
    }
    // a value on a bound goes to that bound's bucket (Prometheus's le)
    EXPECT_EQ(h.count(0), 3u);   // 0.5, 1, -5
    EXPECT_EQ(h.count(1), 2u);   // 1.5, 2
    EXPECT_EQ(h.count(2), 1u);   // 3
    EXPECT_EQ(h.count(3), 1u);   // 8
    EXPECT_EQ(h.count(4), 1u);   // 9
    EXPECT_EQ(h.count(), 8u);
    EXPECT_EQ(h.sum(), 0.5 + 1 + 1.5 + 2 + 3 + 8 + 9 - 5);
    EXPECT_THROW(h.count(5), std::out_of_range);
    EXPECT_THROW(h.upper_bound(5), std::out_of_range);
    // Prometheus's quantile: rank 4 of 8 falls in the bucket (1, 2], one of two in
    EXPECT_EQ(h.quantile(0.5), 1.5);
    EXPECT_EQ(h.quantile(1), 8);     // the bucket of +inf: the largest finite bound
    EXPECT_EQ(h.quantile(0), 0);     // the first bucket from 0
    EXPECT_THROW(h.quantile(2), std::domain_error);
    histogram linear = histogram::linear(0, 10, 3);  // 0, 10, 20, +inf
    linear.add(-1);
    linear.add(5);
    EXPECT_EQ(linear.count(0), 1u);
    EXPECT_EQ(linear.count(1), 1u);
    EXPECT_EQ(linear.quantile(0.25), 0);   // a first bound not above zero is the answer
    // merges of the same bounds
    histogram other = histogram::exponential(1, 2, 4);
    other.add(100);
    h.merge(other);
    EXPECT_EQ(h.count(4), 2u);
    EXPECT_EQ(h.count(), 9u);
    EXPECT_THROW(h.merge(linear), std::invalid_argument);
    // bounds checked
    std::vector<double> unordered = {1, 3, 2};
    std::vector<double> infinite = {1, HUGE_VAL};
    std::vector<double> none;
    EXPECT_THROW(histogram{unordered}, std::invalid_argument);
    EXPECT_THROW(histogram{infinite}, std::invalid_argument);
    EXPECT_THROW(histogram::linear(0, 0, 3), std::invalid_argument);
    EXPECT_THROW(histogram::exponential(0, 2, 3), std::invalid_argument);
    EXPECT_THROW(histogram::exponential(1, 1, 3), std::invalid_argument);
    EXPECT_THROW(histogram::linear(0, 1, 0), std::invalid_argument);
    histogram all(none);
    EXPECT_EQ(all.bucket_count(), 1u);
    all.add(3);
    EXPECT_EQ(all.quantile(0.5), HUGE_VAL);
    EXPECT_TRUE(std::isnan(histogram(none).quantile(0.5)));
    // the counts against a straightforward count over random values
    std::mt19937_64 rng(53);
    std::lognormal_distribution<double> d(0, 2);
    histogram e = histogram::exponential(0.01, 1.5, 30);
    std::vector<uint64_t> want(31);
    for (int i = 0; i < 100000; ++i) {
        double x = d(rng);
        e.add(x);
        size_t b = 0;
        while (b < 30 && x > e.upper_bound(b)) {
            ++b;
        }
        ++want[b];
    }
    for (size_t b = 0; b < 31; ++b) {
        ASSERT_EQ(e.count(b), want[b]) << b;
    }
}

//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/detail/os.h"
#include "../core/slice.h"
#include "../core/vector.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <vector>

// Statistics: accumulators that take numbers one at a time and are merged
// (one to a thread, then combined), and the statistics of a whole sequence.
// summary — count, mean, variance, skewness, kurtosis, min, max (Welford's
// update and Terriberry's for the third and fourth moments, Chan's formulas
// for a merge); paired_summary — covariance, correlation and the
// least-squares line of pairs; t_digest — streaming quantiles (Dunning's
// merging t-digest); histogram — counts over fixed buckets (Prometheus's).
// mean, median, quantile and mode of a sequence as Python's statistics module
// gives them.
//
// An accumulator's statistic that is undefined for what it holds — the mean
// of nothing, the variance of one value — is NaN, since a dashboard asks
// before the first value comes; the functions of a whole sequence refuse an
// empty one (domain_error), as Python's do. An accumulator is a value (a
// copy is a snapshot) and not for sharing between threads.
namespace sgcl::math {
    namespace detail {
        constexpr double StatsNaN = std::numeric_limits<double>::quiet_NaN();

        // A sum with Neumaier's compensation: the lost low bits of each step
        // kept apart and added at the end
        struct CompensatedSum {
            double sum = 0;
            double carry = 0;

            SGCL_INLINE_HOT void add(double x) noexcept {
                double t = sum + x;
                if (std::fabs(sum) >= std::fabs(x)) {
                    carry += (sum - t) + x;
                } else {
                    carry += (x - t) + sum;
                }
                sum = t;
            }

            SGCL_INLINE_HOT double value() const noexcept {
                return sum + carry;
            }
        };
    }

    // The moments of numbers added one at a time: count, sum, mean, variance,
    // skewness, kurtosis, min and max, in a few doubles. The moments are kept
    // of the values less the first one, so that values far from zero and near
    // each other (a timestamp, 1e9 and a fraction) lose none of their spread
    class summary {
    public:
        summary() noexcept = default;

        // Every value of the sequence added
        explicit summary(const slice<const double>& values) noexcept {
            for (double x : values) {
                add(x);
            }
        }

        // One value more: the moments updated by Welford's and Terriberry's
        // formulas, with no sum of squares to lose its digits
        void add(double x) noexcept {
            // the moments of x less the first value: values far from zero and
            // near each other (1e9 + a fraction) keep their digits
            if (!_n) {
                _shift = std::isfinite(x) ? x : 0;
            }
            double n1 = double(_n);
            ++_n;
            double n = double(_n);
            double delta = (x - _shift) - _mean;
            double delta_n = delta / n;
            double delta_n2 = delta_n * delta_n;
            double term = delta * delta_n * n1;
            _mean += delta_n;
            _m4 += term * delta_n2 * (n * n - 3 * n + 3) + 6 * delta_n2 * _m2 - 4 * delta_n * _m3;
            _m3 += term * delta_n * (n - 2) - 3 * delta_n * _m2;
            _m2 += term;
            _sum.add(x);
            _min = x < _min ? x : _min;
            _max = x > _max ? x : _max;
        }

        // The values of another added, as if one at a time (Chan's, Pébay's
        // formulas for the moments of a union)
        void merge(const summary& other) noexcept {
            if (!other._n) {
                return;
            }
            if (!_n) {
                *this = other;
                return;
            }
            double na = double(_n);
            double nb = double(other._n);
            double n = na + nb;
            // the other's mean brought to this one's shift
            double delta = (other._shift - _shift) + (other._mean - _mean);
            double d2 = delta * delta;
            double m2 = _m2 + other._m2 + d2 * na * nb / n;
            double m3 = _m3 + other._m3 + d2 * delta * na * nb * (na - nb) / (n * n) + 3 * delta * (na * other._m2 - nb * _m2) / n;
            double m4 = _m4 + other._m4 + d2 * d2 * na * nb * (na * na - na * nb + nb * nb) / (n * n * n)
                      + 6 * d2 * (na * na * other._m2 + nb * nb * _m2) / (n * n) + 4 * delta * (na * other._m3 - nb * _m3) / n;
            _mean += delta * nb / n;
            _m2 = m2;
            _m3 = m3;
            _m4 = m4;
            _n += other._n;
            _sum.add(other._sum.sum);
            _sum.add(other._sum.carry);
            _min = std::min(_min, other._min);
            _max = std::max(_max, other._max);
        }

        SGCL_INLINE_HOT uint64_t count() const noexcept {
            return _n;
        }

        // The sum, compensated (Neumaier)
        SGCL_INLINE_HOT double sum() const noexcept {
            return _sum.value();
        }

        // NaN when empty
        SGCL_INLINE_HOT double mean() const noexcept {
            return _n ? _shift + _mean : detail::StatsNaN;
        }

        // The sample's variance, over n - 1; NaN below two values
        SGCL_INLINE_HOT double variance() const noexcept {
            return _n > 1 ? _m2 / double(_n - 1) : detail::StatsNaN;
        }

        // The population's, over n; NaN when empty
        SGCL_INLINE_HOT double population_variance() const noexcept {
            return _n ? _m2 / double(_n) : detail::StatsNaN;
        }

        SGCL_INLINE_HOT double stddev() const noexcept {
            return std::sqrt(variance());
        }

        SGCL_INLINE_HOT double population_stddev() const noexcept {
            return std::sqrt(population_variance());
        }

        // g1 = m3 / m2^(3/2) of the central moments (the population's,
        // scipy's default); NaN when empty or constant
        double skewness() const noexcept {
            if (!_n || _m2 == 0) {
                return detail::StatsNaN;
            }
            return std::sqrt(double(_n)) * _m3 / std::pow(_m2, 1.5);
        }

        // The excess kurtosis g2 = m4 / m2² - 3 (the population's, scipy's
        // default: 0 for a normal distribution); NaN when empty or constant
        double kurtosis() const noexcept {
            if (!_n || _m2 == 0) {
                return detail::StatsNaN;
            }
            return double(_n) * _m4 / (_m2 * _m2) - 3;
        }

        // +∞ and -∞ when empty; a NaN added is never the least or the most
        SGCL_INLINE_HOT double min() const noexcept {
            return _min;
        }

        SGCL_INLINE_HOT double max() const noexcept {
            return _max;
        }

    private:
        uint64_t _n = 0;
        double _shift = 0;   // the first value: the moments are of x - _shift
        double _mean = 0;
        double _m2 = 0;
        double _m3 = 0;
        double _m4 = 0;
        detail::CompensatedSum _sum;
        double _min = std::numeric_limits<double>::infinity();
        double _max = -std::numeric_limits<double>::infinity();
    };

    // The joint moments of pairs added one at a time: the means, the
    // covariance, Pearson's correlation, the least-squares line
    class paired_summary {
    public:
        paired_summary() noexcept = default;

        void add(double x, double y) noexcept {
            if (!_n) {
                _shift_x = std::isfinite(x) ? x : 0;
                _shift_y = std::isfinite(y) ? y : 0;
            }
            x -= _shift_x;
            y -= _shift_y;
            ++_n;
            double n = double(_n);
            double dx = x - _mean_x;
            _mean_x += dx / n;
            double dy = y - _mean_y;
            _mean_y += dy / n;
            _cxy += dx * (y - _mean_y);
            _m2x += dx * (x - _mean_x);
            _m2y += dy * (y - _mean_y);
        }

        void merge(const paired_summary& other) noexcept {
            if (!other._n) {
                return;
            }
            if (!_n) {
                *this = other;
                return;
            }
            double na = double(_n);
            double nb = double(other._n);
            double n = na + nb;
            double dx = (other._shift_x - _shift_x) + (other._mean_x - _mean_x);
            double dy = (other._shift_y - _shift_y) + (other._mean_y - _mean_y);
            _cxy += other._cxy + dx * dy * na * nb / n;
            _m2x += other._m2x + dx * dx * na * nb / n;
            _m2y += other._m2y + dy * dy * na * nb / n;
            _mean_x += dx * nb / n;
            _mean_y += dy * nb / n;
            _n += other._n;
        }

        SGCL_INLINE_HOT uint64_t count() const noexcept {
            return _n;
        }

        SGCL_INLINE_HOT double mean_x() const noexcept {
            return _n ? _shift_x + _mean_x : detail::StatsNaN;
        }

        SGCL_INLINE_HOT double mean_y() const noexcept {
            return _n ? _shift_y + _mean_y : detail::StatsNaN;
        }

        // The sample's covariance, over n - 1; NaN below two pairs
        SGCL_INLINE_HOT double covariance() const noexcept {
            return _n > 1 ? _cxy / double(_n - 1) : detail::StatsNaN;
        }

        SGCL_INLINE_HOT double population_covariance() const noexcept {
            return _n ? _cxy / double(_n) : detail::StatsNaN;
        }

        // Pearson's correlation, in [-1, 1]; NaN when either is constant
        double correlation() const noexcept {
            if (_n < 2 || _m2x == 0 || _m2y == 0) {
                return detail::StatsNaN;
            }
            return std::clamp(_cxy / std::sqrt(_m2x * _m2y), -1.0, 1.0);
        }

        // The least-squares line y = slope·x + intercept (Python's
        // linear_regression); NaN when x is constant
        SGCL_INLINE_HOT double slope() const noexcept {
            return _n > 1 && _m2x != 0 ? _cxy / _m2x : detail::StatsNaN;
        }

        SGCL_INLINE_HOT double intercept() const noexcept {
            return mean_y() - slope() * mean_x();
        }

    private:
        uint64_t _n = 0;
        double _shift_x = 0;   // the first pair: the moments are of the pairs less it
        double _shift_y = 0;
        double _mean_x = 0;
        double _mean_y = 0;
        double _m2x = 0;
        double _m2y = 0;
        double _cxy = 0;
    };

    // Streaming quantiles by Dunning's merging t-digest (add, merge and cdf
    // noexcept: their only failure is a managed allocation refused, which
    // ends the program): the values kept as
    // centroids (a mean and a weight) sorted by mean, small at the tails and
    // larger in the middle as the scale function k(q) = δ/2π · asin(2q - 1)
    // allows — a centroid spans at most one unit of k — so that the error of
    // a quantile is about 1/δ of the rank in the middle and far less at the
    // ends; new values gather in a buffer and are merged in one pass when it
    // fills or a question is asked. About 2δ centroids; two digests merge.
    // The least and the largest value are kept exactly.
    class t_digest {
    public:
        // compression δ, 10 to 10000: 100 gives about 1% of rank error in the
        // middle; outside the range is invalid_argument
        explicit t_digest(double compression = 100)
        : _compression(compression) {
            if (!(compression >= 10 && compression <= 10000)) {
                throw invalid_argument("sgcl::math::t_digest: a compression outside 10 to 10000");
            }
            _buffer_capacity = size_t(std::ceil(compression * 5));
            _buffer.reserve(_buffer_capacity);
        }

        // One value more; NaN is ignored (it has no place in an order), an
        // infinity counted apart (it has a place at an end, but no centroid
        // can take its mean)
        void add(double x) noexcept {
            if (std::isnan(x)) {
                return;
            }
            ++_count;
            if (std::isinf(x)) {
                ++(x < 0 ? _negative_infinities : _positive_infinities);
                return;
            }
            _buffer.push_back({x, 1});
            _min = x < _min ? x : _min;
            _max = x > _max ? x : _max;
            if (_buffer.size() >= _buffer_capacity) {
                _compress();
            }
        }

        // The values of another added (its centroids, rounded to this one's
        // compression by the merge)
        void merge(const t_digest& other) noexcept {
            if (!other._count) {
                return;
            }
            other._compress();
            for (const auto& c : other._centroids) {
                _buffer.push_back(c);
            }
            _count += other._count;
            _negative_infinities += other._negative_infinities;
            _positive_infinities += other._positive_infinities;
            _min = std::min(_min, other._min);
            _max = std::max(_max, other._max);
            _compress();
        }

        SGCL_INLINE_HOT uint64_t count() const noexcept {
            return _count;
        }

        // The least and the largest value (an infinity when one was added);
        // +∞ and -∞ when empty
        SGCL_INLINE_HOT double min() const noexcept {
            return _negative_infinities ? -std::numeric_limits<double>::infinity()
                                        : _finite() ? _min : _positive_infinities ? std::numeric_limits<double>::infinity()
                                                                                  : _min;
        }

        SGCL_INLINE_HOT double max() const noexcept {
            return _positive_infinities ? std::numeric_limits<double>::infinity()
                                        : _finite() ? _max : _negative_infinities ? -std::numeric_limits<double>::infinity()
                                                                                  : _max;
        }

        // The value with the fraction q of the values below it, q in [0, 1]
        // (domain_error outside): linear between the centres of the centroids,
        // the least and the largest value at the ends; NaN when empty
        double quantile(double q) const {
            if (!(q >= 0 && q <= 1)) {
                throw domain_error("sgcl::math::t_digest::quantile: q outside [0, 1]");
            }
            if (!_count) {
                return detail::StatsNaN;
            }
            double t = q * double(_count);
            double negative = double(_negative_infinities);
            double finite = double(_finite());
            if (_negative_infinities && (t < negative || q == 0 || (!finite && t <= negative))) {
                return -std::numeric_limits<double>::infinity();
            }
            if (_positive_infinities && (t > negative + finite || q == 1 || !finite)) {
                return std::numeric_limits<double>::infinity();
            }
            _compress();
            const auto& c = _centroids;
            t = std::clamp(t - negative, 0.0, finite);
            if (c.size() == 1) {
                return _between(0, _min, finite, _max, t);
            }
            // the centres: (weight before + weight/2, mean); a centroid of one
            // value is that value exactly at its centre
            double before = 0;
            double last_position = 0;
            double last_value = _min;
            for (size_t i = 0; i < c.size(); ++i) {
                double position = before + c[i].weight / 2;
                if (t < position) {
                    return _between(last_position, last_value, position, c[i].mean, t);
                }
                last_position = position;
                last_value = c[i].mean;
                before += c[i].weight;
            }
            return _between(last_position, last_value, finite, _max, t);
        }

        // The fraction of the values below x, the inverse of quantile: 0
        // below the least value, 1 from the largest on (an infinity added
        // counts below or above every number); NaN when empty
        double cdf(double x) const noexcept {
            if (!_count || std::isnan(x)) {
                return detail::StatsNaN;
            }
            double total = double(_count);
            double negative = double(_negative_infinities);   // below every number
            if (x == std::numeric_limits<double>::infinity()) {
                return 1;
            }
            if (x == -std::numeric_limits<double>::infinity()) {
                return 0;
            }
            if (!_finite() || x < _min) {
                return negative / total;
            }
            if (x >= _max) {
                return (negative + double(_finite())) / total;
            }
            _compress();
            const auto& c = _centroids;
            double finite = double(_finite());
            double before = 0;
            double last_position = 0;
            double last_value = _min;
            for (size_t i = 0; i < c.size(); ++i) {
                double position = before + c[i].weight / 2;
                if (x < c[i].mean) {
                    return (negative + _position(last_position, last_value, position, c[i].mean, x)) / total;
                }
                last_position = position;
                last_value = c[i].mean;
                before += c[i].weight;
            }
            return (negative + _position(last_position, last_value, finite, _max, x)) / total;
        }

        // The compression it was made with
        SGCL_INLINE_HOT double compression() const noexcept {
            return _compression;
        }

    private:
        struct Centroid {
            double mean;
            double weight;
        };

        // value at t on the line through (p0, v0) and (p1, v1): std::lerp,
        // monotonic and with no difference of the values to overflow
        static double _between(double p0, double v0, double p1, double v1, double t) noexcept {
            if (p1 <= p0) {
                return v1;
            }
            return std::lerp(v0, v1, std::clamp((t - p0) / (p1 - p0), 0.0, 1.0));
        }

        // its inverse: for values near either end of the doubles their halves
        // subtracted, so that the difference does not overflow
        static double _position(double p0, double v0, double p1, double v1, double x) noexcept {
            if (v1 <= v0) {
                return p1;
            }
            bool huge = std::fabs(v0) > 1e300 || std::fabs(v1) > 1e300;
            double f = huge ? (x / 2 - v0 / 2) / (v1 / 2 - v0 / 2) : (x - v0) / (v1 - v0);
            if (!(f >= 0)) {
                f = 0;   // NaN: a span the halves made nothing of
            }
            return p0 + (p1 - p0) * std::min(f, 1.0);
        }

        double _k(double q) const noexcept {
            return _compression / (2 * 3.14159265358979323846) * std::asin(2 * q - 1);
        }

        double _q_of(double k) const noexcept {
            double a = 2 * 3.14159265358979323846 * k / _compression;
            if (a >= 3.14159265358979323846 / 2) {
                return 1;
            }
            return (std::sin(a) + 1) / 2;
        }

        // The buffer and the centroids sorted together and merged in one pass:
        // a centroid grows while it stays within one unit of k of where it began
        void _compress() const noexcept {
            if (_buffer.empty()) {
                return;
            }
            for (const auto& c : _centroids) {
                _buffer.push_back(c);
            }
            std::sort(_buffer.begin(), _buffer.end(), [](const Centroid& a, const Centroid& b) {
                return a.mean < b.mean;
            });
            double total = 0;
            for (const auto& c : _buffer) {
                total += c.weight;
            }
            _centroids.clear();
            Centroid current = _buffer[0];
            double q0 = 0;
            double limit = _q_of(_k(q0) + 1);
            for (size_t i = 1; i < _buffer.size(); ++i) {
                const Centroid& next = _buffer[i];
                double q = q0 + (current.weight + next.weight) / total;
                if (q <= limit) {
                    // the weighted mean as a sum of the two parts, which no
                    // difference of means near the ends of the doubles overflows
                    double weight = current.weight + next.weight;
                    double mean = current.mean * (current.weight / weight) + next.mean * (next.weight / weight);
                    current.mean = std::clamp(mean, current.mean, next.mean);   // sorted: current.mean <= next.mean
                    current.weight = weight;
                } else {
                    _centroids.push_back(current);
                    q0 += current.weight / total;
                    limit = _q_of(_k(q0) + 1);
                    current = next;
                }
            }
            _centroids.push_back(current);
            _buffer.clear();
        }

        // the values that are neither NaN nor infinite
        SGCL_INLINE_HOT uint64_t _finite() const noexcept {
            return _count - _negative_infinities - _positive_infinities;
        }

        double _compression;
        size_t _buffer_capacity;
        uint64_t _count = 0;
        uint64_t _negative_infinities = 0;
        uint64_t _positive_infinities = 0;
        double _min = std::numeric_limits<double>::infinity();
        double _max = -std::numeric_limits<double>::infinity();
        mutable vector<Centroid> _centroids;
        mutable vector<Centroid> _buffer;
    };

    // Counts over fixed buckets, Prometheus's histogram: bucket i counts the
    // values above upper_bound(i - 1) and up to and including upper_bound(i),
    // the last one everything above the last bound (+∞); the count and the
    // sum of every value besides
    class histogram {
    public:
        // The upper bounds of the buckets, ascending and finite (else
        // invalid_argument); a bucket of +∞ follows them. No bounds: one
        // bucket of everything
        explicit histogram(const slice<const double>& upper_bounds)
        : _bounds(upper_bounds.begin(), upper_bounds.end())
        , _counts(upper_bounds.size() + 1, uint64_t(0)) {
            for (size_t i = 0; i < _bounds.size(); ++i) {
                if (!std::isfinite(_bounds[i]) || (i && !(_bounds[i] > _bounds[i - 1]))) {
                    throw invalid_argument("sgcl::math::histogram: bounds not finite and ascending");
                }
            }
        }

        // count buckets start, start + width, … (Prometheus's LinearBuckets);
        // width above zero and count at least one, else invalid_argument
        static histogram linear(double start, double width, size_t count) {
            if (!(width > 0) || count < 1) {
                throw invalid_argument("sgcl::math::histogram::linear: a width not above zero or no buckets");
            }
            std::vector<double> bounds(count);
            for (size_t i = 0; i < count; ++i) {
                bounds[i] = start + double(i) * width;
            }
            return histogram(bounds);
        }

        // count buckets start, start·factor, start·factor², … (Prometheus's
        // ExponentialBuckets, each the one before times the factor); start
        // above zero, factor above one, count at least one
        static histogram exponential(double start, double factor, size_t count) {
            if (!(start > 0) || !(factor > 1) || count < 1) {
                throw invalid_argument("sgcl::math::histogram::exponential: a start not above zero, a factor not above one or no buckets");
            }
            std::vector<double> bounds(count);
            double b = start;
            for (size_t i = 0; i < count; ++i) {
                bounds[i] = b;
                b *= factor;
            }
            return histogram(bounds);
        }

        // One value into its bucket, the first whose bound is not below it;
        // NaN is ignored
        void add(double x) noexcept {
            if (std::isnan(x)) {
                return;
            }
            size_t i = size_t(std::lower_bound(_bounds.begin(), _bounds.end(), x) - _bounds.begin());
            ++_counts[i];
            ++_count;
            _sum.add(x);
        }

        // The counts of another of the same bounds added (invalid_argument
        // for other bounds)
        void merge(const histogram& other) {
            if (other._bounds.size() != _bounds.size() || !std::equal(_bounds.begin(), _bounds.end(), other._bounds.begin())) {
                throw invalid_argument("sgcl::math::histogram::merge: other bounds");
            }
            for (size_t i = 0; i < _counts.size(); ++i) {
                _counts[i] += other._counts[i];
            }
            _count += other._count;
            _sum.add(other._sum.sum);
            _sum.add(other._sum.carry);
        }

        // The buckets, the bounds and the one of +∞
        SGCL_INLINE_HOT size_t bucket_count() const noexcept {
            return _counts.size();
        }

        // The bound of a bucket, +∞ for the last; out_of_range past it
        double upper_bound(size_t bucket) const {
            _check(bucket);
            return bucket < _bounds.size() ? _bounds[bucket] : std::numeric_limits<double>::infinity();
        }

        // The values in a bucket, not counting the ones below (Prometheus's
        // counts are cumulative: the sum of these up to the bucket)
        uint64_t count(size_t bucket) const {
            _check(bucket);
            return _counts[bucket];
        }

        // Every value counted
        SGCL_INLINE_HOT uint64_t count() const noexcept {
            return _count;
        }

        // Their sum, compensated
        SGCL_INLINE_HOT double sum() const noexcept {
            return _sum.value();
        }

        // Prometheus's histogram_quantile: the bucket the rank q·count falls
        // in, and linear within it from the bound below (0 for the first
        // bucket of a positive bound) to its own; the largest finite bound
        // for the bucket of +∞; NaN when empty. q outside [0, 1] is
        // domain_error
        double quantile(double q) const {
            if (!(q >= 0 && q <= 1)) {
                throw domain_error("sgcl::math::histogram::quantile: q outside [0, 1]");
            }
            if (!_count) {
                return detail::StatsNaN;
            }
            double rank = q * double(_count);
            uint64_t cumulative = 0;
            size_t b = 0;
            for (; b < _counts.size(); ++b) {
                if (double(cumulative + _counts[b]) >= rank && _counts[b]) {
                    break;
                }
                cumulative += _counts[b];
            }
            if (b >= _bounds.size()) {
                return _bounds.empty() ? std::numeric_limits<double>::infinity() : _bounds[_bounds.size() - 1];
            }
            if (b == 0 && _bounds[0] <= 0) {
                return _bounds[0];
            }
            double start = b ? _bounds[b - 1] : 0;
            double end = _bounds[b];
            return start + (end - start) * ((rank - double(cumulative)) / double(_counts[b]));
        }

    private:
        void _check(size_t bucket) const {
            if (bucket >= _counts.size()) {
                throw out_of_range("sgcl::math::histogram: a bucket past the last");
            }
        }

        vector<double> _bounds;
        vector<uint64_t> _counts;
        uint64_t _count = 0;
        detail::CompensatedSum _sum;
    };

    namespace detail {
        inline void stats_check(const slice<const double>& values, const char* what) {
            if (values.empty()) {
                throw domain_error(std::string("sgcl::math::") + what + ": no values");
            }
        }

        // The values copied for ordering, refusing NaN (one pass)
        inline std::vector<double> stats_copy(const slice<const double>& values, const char* what) {
            stats_check(values, what);
            std::vector<double> v(values.begin(), values.end());
            for (double x : v) {
                if (std::isnan(x)) {
                    throw domain_error(std::string("sgcl::math::") + what + ": a NaN has no place in an order");
                }
            }
            return v;
        }
    }

    // The arithmetic mean, from a compensated sum (Neumaier): within a unit
    // or two of the last place of the exact mean, as Python's fmean; no
    // values is domain_error
    inline double mean(const slice<const double>& values) {
        detail::stats_check(values, "mean");
        detail::CompensatedSum s;
        for (double x : values) {
            s.add(x);
        }
        return s.value() / double(values.size());
    }

    // The middle value, or the mean of the two in the middle of an even count
    // (Python's median); no values or a NaN among them is domain_error
    inline double median(const slice<const double>& values) {
        std::vector<double> v = detail::stats_copy(values, "median");
        size_t n = v.size();
        auto middle = v.begin() + ptrdiff_t(n / 2);
        std::nth_element(v.begin(), middle, v.end());
        double high = *middle;
        if (n % 2) {
            return high;
        }
        double low = *std::max_element(v.begin(), middle);
        if (std::isinf(low) || std::isinf(high)) {
            return (low + high) / 2;   // -∞ with a number is -∞; -∞ with +∞ is NaN, as Python's
        }
        return std::midpoint(low, high);   // (low + high) / 2, Python's, but never past the two
    }

    // The q-quantile, linear between the order statistics: with the values
    // sorted x_0 ≤ … ≤ x_(n-1), h = q·(n - 1), x_⌊h⌋ + (h - ⌊h⌋)·(x_⌊h⌋+1 -
    // x_⌊h⌋) — Python's quantiles(method='inclusive'), numpy's default, R's
    // type 7, Excel's PERCENTILE.INC. q outside [0, 1], no values or a NaN
    // among them is domain_error
    inline double quantile(const slice<const double>& values, double q) {
        if (!(q >= 0 && q <= 1)) {
            throw domain_error("sgcl::math::quantile: q outside [0, 1]");
        }
        std::vector<double> v = detail::stats_copy(values, "quantile");
        double h = q * double(v.size() - 1);
        auto i = size_t(h);
        double f = h - double(i);
        std::nth_element(v.begin(), v.begin() + ptrdiff_t(i), v.end());
        double low = v[i];
        if (f == 0 || i + 1 >= v.size()) {
            return low;
        }
        double high = *std::min_element(v.begin() + ptrdiff_t(i + 1), v.end());
        if (std::isinf(low)) {
            return low;   // between an infinity and anything, the infinity
        }
        if (std::isinf(high)) {
            return high;
        }
        return std::lerp(low, high, f);   // within [low, high] where high - low would overflow
    }

    // The most common value; of several equally common, the first in the
    // sequence (Python's mode); no values or a NaN among them is domain_error
    inline double mode(const slice<const double>& values) {
        std::vector<double> v = detail::stats_copy(values, "mode");
        std::unordered_map<double, size_t> counts;
        counts.reserve(v.size());
        size_t best = 0;
        double value = v[0];
        for (double x : v) {
            size_t c = ++counts[x == 0 ? 0.0 : x];   // -0 and +0 one value
            if (c > best) {
                best = c;
                value = x;
            }
        }
        // the first in the sequence of those as common as the most: the scan
        // above found the one that reached the count first; Python's is the
        // first seen of them, asked again in order
        for (double x : v) {
            if (counts[x == 0 ? 0.0 : x] == best) {
                return x;
            }
        }
        return value;
    }
}

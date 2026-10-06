//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The accumulators of statistics.h fed any doubles (the input's bytes, eight
// to a value, NaN, infinities and subnormals among them; the first byte sets
// the t-digest's compression and where the values are split for a merge).
// What must hold:
//   - summary: the count; min and max the least and the largest of the values
//     that are not NaN; a merge of the two halves has the count, min, max and
//     (for finite values of a sane range) a mean and a variance near the whole's;
//   - t_digest: the count of the values that are not NaN; every quantile within
//     [min, max], monotonic in q, the ends exactly min and max; the cdf in
//     [0, 1] and monotonic; a merge of the halves counts every value;
//   - histogram: the counts of the buckets add up to the count; a merge too;
//   - the functions of a sequence: the median and every quantile within the
//     values' range, NaN refused.
// Built with libFuzzer (tests/fuzz/run.sh tests/math/fuzz/statistics_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/math/math.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {
    using namespace sgcl;
    using namespace sgcl::math;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 8 * 4096 + 1) {
        return 0;
    }
    double compression = 10 + (data[0] % 64) * 5;
    std::vector<double> values((size - 1) / 8);
    for (size_t i = 0; i < values.size(); ++i) {
        std::memcpy(&values[i], data + 1 + 8 * i, 8);
    }
    size_t cut = values.empty() ? 0 : data[0] % (values.size() + 1);
    summary whole;
    summary left;
    summary right;
    t_digest digest(compression);
    t_digest dl(compression);
    t_digest dr(compression);
    histogram h = histogram::exponential(1e-3, 3, 20);
    histogram hl = histogram::exponential(1e-3, 3, 20);
    histogram hr = histogram::exponential(1e-3, 3, 20);
    uint64_t numbers = 0;
    double least = HUGE_VAL;
    double most = -HUGE_VAL;
    bool sane = true;
    for (size_t i = 0; i < values.size(); ++i) {
        double x = values[i];
        whole.add(x);
        (i < cut ? left : right).add(x);
        digest.add(x);
        (i < cut ? dl : dr).add(x);
        h.add(x);
        (i < cut ? hl : hr).add(x);
        if (!std::isnan(x)) {
            ++numbers;
            least = std::min(least, x);
            most = std::max(most, x);
            sane = sane && std::fabs(x) < 1e100 && (x == 0 || std::fabs(x) > 1e-100);
        } else {
            sane = false;
        }
    }
    check(whole.count() == values.size());
    check(whole.min() == least && whole.max() == most);
    left.merge(right);
    check(left.count() == whole.count() && left.min() == whole.min() && left.max() == whole.max());
    if (sane && numbers > 1) {
        double spread = std::max(std::fabs(least), std::fabs(most));
        check(std::fabs(left.mean() - whole.mean()) <= 1e-9 * spread);
        check(std::fabs(left.variance() - whole.variance()) <= 1e-6 * spread * spread + 1e-9 * whole.variance());
    }
    check(digest.count() == numbers);
    dl.merge(dr);
    check(dl.count() == numbers);
    if (numbers) {
        check(digest.quantile(0) == least && digest.quantile(1) == most);
        double last = -HUGE_VAL;
        double last_cdf = 0;
        for (int i = 0; i <= 64; ++i) {
            double q = i / 64.0;
            double v = digest.quantile(q);
            check(v >= least && v <= most && v >= last);
            last = v;
            double c = digest.cdf(v);
            check(c >= 0 && c <= 1 && c >= last_cdf - 1e-9);
            last_cdf = c;
            double m = dl.quantile(q);
            check(m >= least && m <= most);
        }
    }
    uint64_t total = 0;
    for (size_t b = 0; b < h.bucket_count(); ++b) {
        total += h.count(b);
    }
    check(total == h.count() && h.count() == numbers);
    hl.merge(hr);
    check(hl.count() == h.count());
    for (size_t b = 0; b < h.bucket_count(); ++b) {
        check(hl.count(b) == h.count(b));
    }
    if (numbers == values.size() && numbers) {
        double m = median(values);
        check((m >= least && m <= most) || (std::isnan(m) && std::isinf(least) && std::isinf(most)));
        double q = quantile(values, (data[0] % 101) / 100.0);
        check(q >= least && q <= most);
        double mo = mode(values);
        check(mo >= least && mo <= most);
    } else if (numbers != values.size()) {
        bool threw = false;
        try {
            (void)median(values);
        } catch (const domain_error&) {
            threw = true;
        }
        check(threw);
    }
    return 0;
}

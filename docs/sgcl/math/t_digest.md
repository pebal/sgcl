[sgcl](../README.md) › [math](README.md)

# sgcl::math::t_digest

```cpp
#include "sgcl/math/statistics.h"   // or "sgcl/math.h"

namespace sgcl::math {
    class t_digest;
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::math::t_digest` estimates quantiles of numbers taken one at a time, in bounded memory: the median, the 99th
percentile of a million latencies, and the inverse, the fraction of values below a number. It is Ted Dunning's merging
t-digest: the values kept as centroids — a mean and a weight — sorted by mean, small at the ends and larger in the
middle as the scale function k(q) = δ/2π · asin(2q − 1) allows (a centroid spans at most one unit of k), so that the
error of a quantile is a fraction of a percent of the rank in the middle and far less at the tails, where latencies
matter. Two digests merge, so each thread keeps its own.

## Rules

- **Compression** δ (10 to 10000, 100 by default) sets the size and the error: about 2δ centroids; at δ = 100, over
  a million values of a normal, an exponential and a uniform distribution, the worst error of rank measured is
  0.3% in the middle and 0.06% at the tails (tests/math/statistics.cpp).
- **Exact ends.** The least and the largest value are kept exactly: `quantile(0)` and `quantile(1)` are they, and a
  digest of a few values (each its own centroid) answers exactly.
- **Buffered.** New values gather in a buffer of 5δ and are merged into the centroids in one sorted pass when it
  fills or a question is asked; a question is const but may do that merge, so a digest is not for sharing between
  threads — one to a thread, then [merge](#member-functions).
- **NaN is ignored**; the infinities are counted apart, at their ends: a quantile that falls among them is an
  infinity, and the centroids hold the numbers between.
- **No exceptions while counting**: `add`, `merge` and `cdf` are `noexcept` — a managed allocation refused ends the
  program, as everywhere in the library.
- **A value**, a copy a snapshot.

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | `explicit t_digest(double compression = 100)`; outside 10 to 10000 `invalid_argument` |
| `add` | `void add(double x) noexcept`: one value more |
| `merge` | `void merge(const t_digest& other) noexcept`: the other's values added |
| `count` | `uint64_t count() const noexcept`: the values added, NaN not among them |
| `min`, `max` | the least and the largest; +∞ and −∞ when empty |
| `quantile` | `double quantile(double q) const`: the value with the fraction q of the values below it, linear between the centres of the centroids; q outside [0, 1] `domain_error`; NaN when empty |
| `cdf` | `double cdf(double x) const noexcept`: the fraction of the values below x, an infinity's own counted; NaN when empty |
| `compression` | `double compression() const noexcept` |

## Complexity

`add` is constant amortized: every 5δ values a sort of the buffer and a pass over it and the centroids. `quantile`
and `cdf` are linear in the centroids (about 2δ). A merge is a pass over both.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::random rng(1);
    math::t_digest latency;
    for (int i = 0; i < 100000; ++i) {
        latency.add(rng.next_exponential() * 10);  // a mean of 10 ms
    }
    println("{} {:.1f} {:.1f} {:.1f}", latency.count(), latency.quantile(0.5), latency.quantile(0.99),
            latency.quantile(0.999));
    println("{:.3f}", latency.cdf(10));
}
```

Sample output:

```text
100000 6.9 46.2 69.4
0.635
```

## See also

- [histogram](histogram.md): counts over fixed buckets
- [quantile](quantile.md): the exact quantile of a whole sequence
- [summary](summary.md): the moments
- [README: math](README.md)

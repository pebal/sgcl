[sgcl](../README.md) › [math](README.md)

# sgcl::math::histogram

```cpp
#include "sgcl/math/statistics.h"   // or "sgcl/math.h"

namespace sgcl::math {
    class histogram;
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::math::histogram` counts numbers over fixed buckets, as a Prometheus histogram does: bucket i counts the values
above the bound of bucket i − 1 and up to and including its own (Prometheus's `le`), and a last bucket counts
everything above the last bound; the count and the sum of every value besides. The buckets are given as their upper
bounds, or made as Prometheus's `LinearBuckets` and `ExponentialBuckets` make them, and
[quantile](#member-functions) estimates a quantile as Prometheus's `histogram_quantile` does. Two histograms of the
same bounds merge.

## Rules

- **The bounds** are finite and ascending (else `invalid_argument`); the bucket of +∞ follows them. No bounds make
  one bucket of everything.
- **Counts are not cumulative**: `count(i)` is the values of bucket i alone; Prometheus's cumulative count of a bound
  is the sum of these up to it.
- **NaN is ignored**; an infinity is a value (−∞ in the first bucket, +∞ in the last).
- **A value**, a copy a snapshot; not for sharing between threads — one to a thread, then [merge](#member-functions).

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | `explicit histogram(const slice<const double>& upper_bounds)` |
| `linear` | `static histogram linear(double start, double width, size_t count)`: `count` bounds `start`, `start + width`, …; a width not above zero or no bounds `invalid_argument` |
| `exponential` | `static histogram exponential(double start, double factor, size_t count)`: `count` bounds `start`, `start·factor`, … (each the one before times the factor); a start not above zero, a factor not above one or no bounds `invalid_argument` |
| `add` | `void add(double x)`: one value into the first bucket whose bound is not below it |
| `merge` | `void merge(const histogram& other)`: the other's counts added; other bounds `invalid_argument` |
| `bucket_count` | the buckets: the bounds and the one of +∞ |
| `upper_bound` | `double upper_bound(size_t bucket) const`: +∞ for the last; past it `out_of_range` |
| `count` | `uint64_t count(size_t bucket) const`: the values of a bucket; `uint64_t count() const`: every value |
| `sum` | `double sum() const`: their sum, compensated |
| `quantile` | `double quantile(double q) const`: the bucket the rank q·count falls in, linear within it from the bound below (0 for a first bucket of a positive bound) to its own; the largest finite bound for the bucket of +∞; NaN when empty; q outside [0, 1] `domain_error` |

## Complexity

`add` is a binary search of the bounds; `quantile` a pass over the buckets; a merge a pass.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::histogram h = math::histogram::exponential(1, 2, 4);  // 1, 2, 4, 8 and +inf
    for (double ms : {0.5, 1.0, 1.5, 2.0, 3.0, 8.0, 9.0}) {
        h.add(ms);
    }
    for (size_t i = 0; i < h.bucket_count(); ++i) {
        println("{} {}", h.upper_bound(i), h.count(i));
    }
    println("{} {} {}", h.count(), h.sum(), h.quantile(0.5));
}
```

Output:

```text
1 2
2 2
4 1
8 1
inf 1
7 25 1.75
```

## See also

- [t_digest](t_digest.md): quantiles without fixed buckets
- [summary](summary.md): the moments
- [README: math](README.md)

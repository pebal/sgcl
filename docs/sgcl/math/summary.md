[sgcl](../README.md) › [math](README.md)

# sgcl::math::summary

```cpp
#include "sgcl/math/statistics.h"   // or "sgcl/math.h"

namespace sgcl::math {
    class summary;
}
```

`sgcl::math::summary` takes numbers one at a time and answers their count, sum, mean, variance and standard
deviation (of the sample and of the population), skewness, kurtosis, least and largest, in a few doubles, without
keeping the numbers. Two summaries merge into the summary of both, so each thread keeps its own and a reader
combines them. Python's `statistics` asks for the whole sequence; Go's standard library has nothing of the kind.

## Rules

- **Stable updates.** The mean and the central moments are updated by Welford's formula and Terriberry's for the
  third and fourth moments, never through a sum of squares that loses its digits; a merge by Chan's and Pébay's
  formulas. The moments are kept of the values less the first one, so values far from zero and near each other (a
  timestamp, 1e9 and a fraction) keep their spread. The sum is compensated (Neumaier).
- **Undefined is NaN.** The mean of nothing, the variance of one value, the skewness and kurtosis of a constant are
  NaN rather than an exception: a dashboard asks before the first value comes. `min` and `max` of nothing are +∞ and
  −∞.
- **NaN goes through the moments** as IEEE arithmetic carries it, and is never the least or the largest.
- **Population skewness and excess kurtosis**: g1 = m3/m2^(3/2) and g2 = m4/m2² − 3 of the central moments, scipy's
  defaults; 0 and 0 for a normal distribution.
- **A value**, a copy a snapshot; not for sharing between threads — one to a thread, then [merge](#member-functions).

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | empty; `explicit summary(const slice<const double>& values)`, every value added |
| `add` | `void add(double x)`: one value more |
| `merge` | `void merge(const summary& other)`: the other's values added, as if one at a time |
| `count` | `uint64_t count() const`: the values added |
| `sum` | `double sum() const`: their sum, compensated |
| `mean` | `double mean() const`: NaN when empty |
| `variance`, `population_variance` | over n − 1 (NaN below two values) and over n |
| `stddev`, `population_stddev` | their square roots |
| `skewness` | `double skewness() const`: g1, NaN when empty or constant |
| `kurtosis` | `double kurtosis() const`: the excess g2, NaN when empty or constant |
| `min`, `max` | the least and the largest; +∞ and −∞ when empty |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::summary s(vector<double>{2, 4, 4, 4, 5, 5, 7, 9});
    println("{} {} {} {}", s.count(), s.mean(), s.population_stddev(), s.stddev());
    println("{:.4f} {:.4f} {} {}", s.skewness(), s.kurtosis(), s.min(), s.max());
    math::summary more;
    more.add(1e9 + 0.5);
    more.add(1e9 + 1.5);
    s.merge(more);
    println("{} {}", s.count(), s.max());
    println(math::summary().mean());
}
```

Output:

```text
8 5 2 2.138089935299395
0.6562 -0.2188 2 9
10 1000000001.5
nan
```

## See also

- [paired_summary](paired_summary.md): covariance and correlation of pairs
- [t_digest](t_digest.md), [histogram](histogram.md): the distribution, streaming
- [mean](mean.md), [median](median.md), [quantile](quantile.md), [mode](mode.md): of a whole sequence
- [README: math](README.md)

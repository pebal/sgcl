[sgcl](../README.md) › [math](README.md)

# sgcl::math::paired_summary

```cpp
#include "sgcl/math/statistics.h"   // or "sgcl/math.h"

namespace sgcl::math {
    class paired_summary;
}
```

`sgcl::math::paired_summary` takes pairs of numbers one at a time and answers their means, their covariance (of the
sample and of the population), Pearson's correlation and the least-squares line `y = slope·x + intercept` — Python's
`statistics.covariance`, `correlation` and `linear_regression` — without keeping the pairs. Two merge into the
summary of both. The co-moments are updated as [summary](summary.md)'s moments are, of the pairs less the first one.
A statistic undefined for what it holds (a correlation with a constant side, anything of fewer than two pairs) is NaN.

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | empty |
| `add` | `void add(double x, double y)`: one pair more |
| `merge` | `void merge(const paired_summary& other)`: the other's pairs added |
| `count` | `uint64_t count() const` |
| `mean_x`, `mean_y` | the means; NaN when empty |
| `covariance`, `population_covariance` | over n − 1 (NaN below two pairs) and over n |
| `correlation` | `double correlation() const`: Pearson's, in [−1, 1]; NaN when a side is constant |
| `slope`, `intercept` | the least-squares line; NaN when x is constant |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::paired_summary p;
    for (int i : {1, 2, 3, 4, 5}) {
        p.add(i, 2.0 * i + 1);
    }
    println("{} {} {} {}", p.covariance(), p.correlation(), p.slope(), p.intercept());
}
```

Output:

```text
5 1 2 1
```

## See also

- [summary](summary.md): the moments of one sequence
- [README: math](README.md)

[sgcl](../README.md) › [math](README.md)

# sgcl::math::quantile

```cpp
double quantile(const slice<const double>& values, double q);
```

The q-quantile of the values, linear between the order statistics: with the values sorted x₀ ≤ … ≤ x₍ₙ₋₁₎ and h = q·(n − 1), x⌊h⌋ + (h − ⌊h⌋)·(x⌊h⌋₊₁ − x⌊h⌋) — Python's `statistics.quantiles(method='inclusive')`, numpy's default, R's type 7, Excel's `PERCENTILE.INC`. `quantile(v, 0)` is the least, `quantile(v, 1)` the largest, `quantile(v, 0.5)` the [median](median.md). Between an infinity and anything the answer is the infinity; between the largest numbers of opposite signs, whose difference overflows, it is still a number between them. The header is `sgcl/math/statistics.h`.

## Parameters

| Parameter | Description |
|---|---|
| `values` | the numbers, in any order |
| `q` | the fraction, in [0, 1] |

## Return value

The quantile.

## Complexity

Linear on average: a copy and two selections.

## Exceptions

- `domain_error` when `q` is outside [0, 1], there are no values or a NaN is among them.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    vector<double> v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    println("{} {} {} {}", math::quantile(v, 0.25), math::quantile(v, 0.5), math::quantile(v, 0.9),
            math::quantile(v, 1));
}
```

Output:

```text
3.25 5.5 9.1 10
```

## See also

- [median](median.md): the middle
- [t_digest](t_digest.md): quantiles streaming, in bounded memory
- [README: math](README.md)

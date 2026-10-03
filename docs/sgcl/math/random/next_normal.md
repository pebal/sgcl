[sgcl](../../README.md) › [math](../README.md) › [random](README.md)

# sgcl::math::random::next_normal

```cpp
double next_normal(double mean = 0, double stddev = 1);
```

A normal (Gaussian) variate of the mean and the standard deviation, by the ziggurat method of Marsaglia and Tsang
(2000) over 128 layers: the low 7 bits of a draw choose the layer and the 57 bits above them are the value and its
sign, so the two are never the same bits. Inside the layer's rectangle, which is nearly always, the value is the
answer; otherwise the wedge is tried against the density, and the bottom layer's tail is sampled by Marsaglia's
method.

The tables of the ziggurat are literals in the source (written by `tools/math_tables.py`), so a seed gives the same
numbers on every platform, which `std::normal_distribution` does not promise, but for the rare draw that falls
outside the rectangles and goes through `std::exp` or `std::log`, whose last bit a platform's library may round
otherwise. The method is this library's own and not Go's: `NormFloat64` from the same stream gives other numbers.

## Parameters

| Parameter | Description |
|---|---|
| `mean` | the mean of the distribution |
| `stddev` | the standard deviation, zero or above |

## Return value

`mean + stddev * z`, `z` a standard normal variate.

## Complexity

Constant on the average: one draw nearly always, a few more and a logarithm or an exponential on the rare draw
outside the rectangles.

## Exceptions

`domain_error` when `stddev` is negative or NaN.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"
#include <cmath>

using namespace sgcl;

int main() {
    math::random r(42);
    println("{:.4f} {:.4f}", r.next_normal(), r.next_normal(100, 15));

    double sum = 0;
    double squares = 0;
    for (int i : range(100000)) {
        double x = r.next_normal(100, 15);
        sum += x;
        squares += x * x;
    }
    double mean = sum / 100000;
    println("mean {:.2f}, deviation {:.2f}", mean, std::sqrt(squares / 100000 - mean * mean));
}
```

Output:

```text
-0.3289 114.7722
mean 99.94, deviation 14.99
```

## See also

- [next_exponential](next_exponential.md): the other ziggurat
- [operator()](operator_call.md): `std::normal_distribution` and the rest of `<random>`
- [sgcl::math::random](README.md)

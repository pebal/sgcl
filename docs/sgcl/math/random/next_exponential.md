[sgcl](../../README.md) › [math](../README.md) › [random](../random.md)

# sgcl::math::random::next_exponential

```cpp
double next_exponential(double rate = 1);
```

An exponential variate of the rate, whose mean is `1 / rate`: the time to the next event of a process with `rate`
events a unit of time. The ziggurat method of Marsaglia and Tsang (2000) over 256 layers: the low 8 bits of a
draw choose the layer and the 56 bits above them are the value, so the two are never the same bits; the bottom
layer's tail is a logarithm.

The tables are literals in the source (`tools/math_tables.py`), so a seed gives the same numbers on every platform,
which `std::exponential_distribution` does not promise, but for the rare draw that falls outside the rectangles
and goes through `std::exp` or `std::log`, whose last bit a platform's library may round otherwise. The method is
this library's own and not Go's: `ExpFloat64` from the same stream gives other numbers.

## Parameters

| Parameter | Description |
|---|---|
| `rate` | the rate, above zero |

## Return value

The variate drawn, zero or above.

## Complexity

Constant on the average: one draw nearly always, a few more and a logarithm or an exponential on the rare draw
outside the rectangles.

## Exceptions

`domain_error` when `rate` is zero, below zero or NaN.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::random r(42);
    println("{:.4f} {:.4f}", r.next_exponential(), r.next_exponential(4));

    double sum = 0;
    for (int i : range(100000)) {
        sum += r.next_exponential(4);
    }
    println("mean {:.3f}", sum / 100000);
}
```

Output:

```text
0.5282 0.2302
mean 0.249
```

## See also

- [next_normal](next_normal.md): the other ziggurat
- [operator()](operator_call.md): the distributions of `<random>`
- [sgcl::math::random](../random.md)

[sgcl](../../README.md) › [concurrent](../README.md) › [count_min_sketch](README.md)

# sgcl::concurrent::count_min_sketch::count_min_sketch

```cpp
explicit count_min_sketch(double epsilon = 0.001, double delta = 0.01);    // (1)
count_min_sketch(const count_min_sketch&) noexcept = default;              // (2)
count_min_sketch(count_min_sketch&&) noexcept = default;                   // (3)
```

1. A sketch whose estimate is within ε·*N* of the true count with a probability of 1 − δ: *w* = ⌈e/ε⌉ counters a row
   and *d* = ⌈ln(1/δ)⌉ rows, every counter zero. The defaults, 0.1% of *N* with 99%, are 2719 × 5 counters, 106 KB.
2. A handle of the same sketch: the copy shares the counters.
3. The same, taken from the other handle, which stands for the same sketch still.

## Parameters

| Parameter | Description |
|---|---|
| `epsilon` | the error as a part of all the counts, ε, in (0, 1) |
| `delta` | the probability of an error past it, δ, in (0, 1) |

## Complexity

- (1) Linear in *w*·*d*: the counters allocated, zero.
- (2–3) Constant.

## Exceptions

- (1) `invalid_argument` for ε or δ outside (0, 1), or NaN.
- (2–3) None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::count_min_sketch c(0.01, 0.001);
    println("{} x {}", c.width(), c.depth());
}
```

Output:

```text
272 x 7
```

## See also

- [with_size](with_size.md): a shape given outright
- [sgcl::concurrent::count_min_sketch](README.md)

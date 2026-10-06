[sgcl](../../README.md) › [math](../README.md) › [fft](README.md)

# sgcl::math::fft::forward

```cpp
void forward(const slice<std::complex<double>>& data) const;    // (1)
void forward(const slice<std::complex<float>>& data) const;     // (2)
```

The forward discrete Fourier transform of the numbers in place: `data[k]` becomes `X_k = Σ_j x_j·e^(−2πi·jk/n)`,
unscaled — the coefficient of the frequency k (k cycles over the n numbers), its magnitude n/2 times the amplitude of
a sine of that frequency, n times a constant at k = 0. Above n/2 the frequencies are the negative ones, k − n.

- (1–2) In double and in float, by the plan's tables of that precision. The slice is any contiguous sequence of
  the plan's length: a `vector`, a `std::vector`, an array, a part of one ([subslice](../../core/slice/subslice.md)).

## Parameters

| Parameter | Description |
|---|---|
| `data` | the n numbers, replaced by their transform |

## Return value

None.

## Complexity

O(n log n) (README).

## Exceptions

- `invalid_argument` when `data` is not of the plan's length; `data` is left as it was.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

#include <complex>

using namespace sgcl;

int main() {
    vector<std::complex<double>> x = {1, 2, 3, 4};
    math::fft(4).forward(x);
    for (auto c : x) {
        println("{} {}", c.real(), c.imag());
    }
}
```

Output:

```text
10 0
-2 2
-2 0
-2 -2
```

## See also

- [inverse](inverse.md): back, scaled by 1/n
- [forward_real](forward_real.md): the transform of real numbers, half the work
- [sgcl::math::fft](README.md)

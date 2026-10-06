[sgcl](../../README.md) › [math](../README.md) › [fft](README.md)

# sgcl::math::fft::inverse

```cpp
void inverse(const slice<std::complex<double>>& data) const;    // (1)
void inverse(const slice<std::complex<float>>& data) const;     // (2)
```

The inverse discrete Fourier transform in place: `data[j]` becomes `x_j = (1/n)·Σ_k X_k·e^(+2πi·jk/n)`, so that
`inverse` after [forward](forward.md) gives the numbers back, to the rounding — numpy's convention. FFTW's and vDSP's
backward transforms are n times this.

- (1–2) In double and in float.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the n coefficients, replaced by the numbers they are the transform of |

## Return value

None.

## Complexity

O(n log n), and one pass for the scaling.

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
    vector<std::complex<float>> x = {{1, 1}, {2, 0.5f}, {-1, -3}};
    math::fft plan(3);
    plan.forward(x);
    plan.inverse(x);
    for (auto c : x) {
        println("{:.4f} {:.4f}", c.real(), c.imag());
    }
}
```

Output:

```text
1.0000 1.0000
2.0000 0.5000
-1.0000 -3.0000
```

## See also

- [forward](forward.md): the forward transform
- [inverse_real](inverse_real.md): back to real numbers
- [sgcl::math::fft](README.md)

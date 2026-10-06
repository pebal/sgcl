[sgcl](../../README.md) › [math](../README.md) › [fft](README.md)

# sgcl::math::fft::forward_real

```cpp
void forward_real(const slice<const double>& in, const slice<std::complex<double>>& out) const;    // (1)
void forward_real(const slice<const float>& in, const slice<std::complex<float>>& out) const;      // (2)
```

The forward transform of n real numbers: their coefficients of the frequencies 0 … n/2, n/2 + 1 of them, into `out`.
The other half is not written, as it is the conjugate of this one (`X_(n−k) = conj X_k` for a real sequence); the
imaginary part of `out[0]`, and of `out[n/2]` for an even n, is zero. For an even n the numbers are transformed as
the complex sequence of their pairs at half the length and split, about half the work of a complex transform; for
an odd n as a complex sequence.

- (1–2) In double and in float. `in` is read only, `out` is written whole; the two may not overlap.

## Parameters

| Parameter | Description |
|---|---|
| `in` | the n real numbers |
| `out` | the n/2 + 1 coefficients |

## Return value

None.

## Complexity

O(n log n), about half of [forward](forward.md)'s for an even n.

## Exceptions

- `invalid_argument` when `in` does not hold the plan's length or `out` does not hold n/2 + 1; nothing is written.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

#include <cmath>
#include <complex>

using namespace sgcl;

int main() {
    // 8 samples of a cosine of frequency 2 plus a constant 1
    vector<double> samples;
    for (int i : range(8)) {
        samples.push_back(1 + std::cos(2 * 3.141592653589793 * 2 * i / 8));
    }
    vector<std::complex<double>> spectrum(5);
    math::fft(8).forward_real(samples, spectrum);
    for (auto c : spectrum) {
        println("{:.3f}", std::abs(c));
    }
}
```

Output:

```text
8.000
0.000
4.000
0.000
0.000
```

## See also

- [inverse_real](inverse_real.md): the numbers back
- [forward](forward.md): the transform of complex numbers
- [sgcl::math::fft](README.md)

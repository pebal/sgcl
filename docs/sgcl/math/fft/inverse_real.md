[sgcl](../../README.md) › [math](../README.md) › [fft](README.md)

# sgcl::math::fft::inverse_real

```cpp
void inverse_real(const slice<const std::complex<double>>& in, const slice<double>& out) const;    // (1)
void inverse_real(const slice<const std::complex<float>>& in, const slice<float>& out) const;      // (2)
```

The n real numbers whose coefficients of the frequencies 0 … n/2 are `in`, scaled by 1/n: what
[forward_real](forward_real.md) wrote gives back its numbers. The coefficients of the other half are taken as the
conjugates of these, and the imaginary part of `in[0]` (and of `in[n/2]` for an even n) as zero, as a real sequence
has them: a spectrum that is not of a real sequence gives the real part of its inverse.

- (1–2) In double and in float. The two may not overlap.

## Parameters

| Parameter | Description |
|---|---|
| `in` | the n/2 + 1 coefficients |
| `out` | the n real numbers |

## Return value

None.

## Complexity

O(n log n), as [forward_real](forward_real.md).

## Exceptions

- `invalid_argument` when `in` does not hold n/2 + 1 or `out` does not hold the plan's length; nothing is written.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

#include <complex>

using namespace sgcl;

int main() {
    // a low-pass filter: the frequencies above 2 taken out
    vector<double> signal = {0, 1, 0, -1, 0, 1, 0, -1, 4, 4, 4, 4, 4, 4, 4, 4};
    math::fft plan(16);
    vector<std::complex<double>> spectrum(9);
    plan.forward_real(signal, spectrum);
    for (int k : range(3, 9)) {
        spectrum[size_t(k)] = 0;
    }
    plan.inverse_real(spectrum, signal);
    println("{:.2f} {:.2f} {:.2f}", signal[0], signal[4], signal[12]);
}
```

Output:

```text
1.64 -0.51 4.51
```

## See also

- [forward_real](forward_real.md): the coefficients of real numbers
- [convolve](../convolve.md): a filter as a convolution
- [sgcl::math::fft](README.md)

[sgcl](../../README.md) › [math](../README.md)

# sgcl::math::fft

```cpp
#include "sgcl/math/fft.h"   // or "sgcl/math.h"

namespace sgcl::math {
    class fft;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::math::fft` is a plan for the discrete Fourier transform of one length: made once, `math::fft plan(n)`, it
holds what every transform of length n needs — the twiddle factors, and for a length with a prime factor above 5 the
chirp of Bluestein's algorithm and its transform — and then transforms any number of sequences of that length, in
place, complex or real, in `double` or in `float`. Go has no FFT in its standard library; the plan is what FFTW's
`fftw_plan` and vDSP's `FFTSetup` are, with every length taken (vDSP takes 2^k times 1, 3, 5 or 15) and both
precisions in one object. The data are the standard's `std::complex<double>` and `std::complex<float>`, in a
[slice](../../core/slice/README.md): a `vector`, a `std::vector`, an array or a part of one.

## Rules

- **Numpy's scaling.** The forward transform is `X_k = Σ x_j·e^(−2πi·jk/n)`, unscaled; the inverse has `e^(+2πi·jk/n)`
  and the factor `1/n`, so `inverse(forward(x))` is `x`. FFTW and vDSP scale neither way (vDSP's real forward
  doubles); a result to compare with theirs is multiplied by n after the inverse.
- **One length.** A slice of another length than the plan's is `invalid_argument`; a length past 2³⁰ is
  `length_error` at the plan. A plan of length 0 transforms the empty sequence.
- **An immutable plan.** A one-word handle to a managed object that nothing changes after the constructor: a copy
  shares it, and any number of threads use one plan at once. A transform allocates nothing for up to 8 KB of data;
  past that, a working buffer of the length outside the managed heap, for the call.
- **The algorithms.** A length whose prime factors are 2, 3 and 5 goes by Stockham's form of Cooley and Tukey's
  algorithm, radix 4 while it can and 2, 3 and 5 after, every stage reading the sequence and writing it in the order
  the next one reads — no bit-reversal pass — on NEON for the butterflies on arm64 (four of float or two of double at
  a time). Any other length goes by Bluestein's algorithm over the next power of two at least 2n − 1, so a prime
  length is O(n log n) too, at about four times the work. A real sequence of an even length is transformed as the
  complex one of half its length and split.
- **Rounding.** The error of a transform grows as the logarithm of its length: a few units of the last place of
  the largest coefficient, times log₂ n, for a power of two; about four times that through Bluestein.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](fft.md) | a plan for a length; a copy shares it |
| `(destructor)` | drops the plan; its object is left to the collector |
| [size](size.md) | the length of the transforms |
| [forward](forward.md) | the forward transform of complex numbers, in place |
| [inverse](inverse.md) | the inverse transform, scaled by 1/n, in place |
| [forward_real](forward_real.md) | n real numbers to the n/2 + 1 coefficients of the frequencies 0 … n/2 |
| [inverse_real](inverse_real.md) | the n real numbers back from their n/2 + 1 coefficients |

## Complexity

O(n log n) for every length: about `5n·log₂ n` floating-point operations for a power of two, a few times that for
a length with factors 3 and 5, and about four transforms of twice the length for one through Bluestein. The plan
costs about as much as one transform to make and holds about `2n` complex numbers in each precision (twice that for
Bluestein).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

#include <cmath>
#include <complex>

using namespace sgcl;

int main() {
    // a sine of 5 cycles in 64 samples: the energy at the bins 5 and 59
    vector<std::complex<double>> x;
    for (int i : range(64)) {
        x.push_back(std::sin(2 * 3.141592653589793 * 5 * i / 64));
    }
    math::fft plan(64);
    plan.forward(x);
    for (int k : {4, 5, 6, 59}) {
        println("{} {:.3f}", k, std::abs(x[size_t(k)]));
    }
    plan.inverse(x);
    println("{:.6f}", x[16].real());
}
```

Output:

```text
4 0.000
5 32.000
6 0.000
59 32.000
1.000000
```

## See also

- [convolve](../convolve.md): the linear convolution through real transforms
- [Benchmarks: fft](../benchmarks.md#fft)
- [README: math](../README.md)

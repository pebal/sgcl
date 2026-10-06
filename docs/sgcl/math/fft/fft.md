[sgcl](../../README.md) › [math](../README.md) › [fft](README.md)

# sgcl::math::fft::fft

```cpp
explicit fft(size_t n);      // (1)
fft(const fft&) noexcept;    // (2), implicitly declared
fft(fft&&) noexcept;         // (3), implicitly declared
```

1. A plan for transforms of length `n`: the length factored into radices 4, 2, 3 and 5 and the twiddle factors of
   every stage computed, each from its own angle in double (no recurrence, whose error grows along a stage) and
   rounded once for the float tables; for an even `n` the same for `n/2` and the factors of the real split; for a
   length with another prime factor, Bluestein's chirp and the transform of its conjugate at the next power of two at
   least `2n − 1`. A length past 2³⁰ is `length_error`; `n` of 0 makes a plan that transforms the empty sequence.
2. A copy shares the plan.
3. A move hands it on; the plan moved from is not to be used but to be assigned or destroyed.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the length of the transforms |

## Complexity

- (1) About one transform of length `n` (four for Bluestein, whose chirp is transformed).
- (2–3) Constant.

## Exceptions

- (1) `length_error` when `n` is past 2³⁰.
- (2–3) None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

#include <complex>

using namespace sgcl;

int main() {
    math::fft plan(1000);  // 8 · 125: radices 4, 2, 5, 5, 5
    math::fft prime(1009);  // a prime: Bluestein's
    math::fft same = plan;
    println("{} {} {}", plan.size(), prime.size(), same.size());
    vector<std::complex<float>> x(1009, std::complex<float>(1, 0));
    prime.forward(x);
    println("{:.1f} {:.1e}", x[0].real(), std::abs(x[1]));
}
```

Sample output:

```text
1000 1009 1000
1009.0 2.3e-05
```

## See also

- [size](size.md): the length
- [forward](forward.md), [forward_real](forward_real.md): what the plan does
- [sgcl::math::fft](README.md)

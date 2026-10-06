[sgcl](../README.md) › [math](README.md)

# sgcl::math::convolve

```cpp
vector<double> convolve(const slice<const double>& a, const slice<const double>& b);    // (1)
vector<float> convolve(const slice<const float>& a, const slice<const float>& b);       // (2)
```

The linear convolution of two sequences, `c_k = Σ_i a_i·b_(k−i)`: `a.size() + b.size() − 1` numbers, empty when
either is empty — the product of two polynomials by their coefficients, a signal through a filter whose response is
the other. While the shorter has at most 48 numbers it is computed directly, as the sum; past that by real
transforms of the next power of two at least the result's length ([fft](fft/README.md)): the two sequences
transformed, their coefficients multiplied, the product transformed back. The header is `sgcl/math/fft.h`.

- (1–2) In double and in float.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the sequences |

## Return value

The convolution.

## Complexity

O(p·q) for the shorter q ≤ 48 of the two lengths, O((p + q) log(p + q)) otherwise.

## Exceptions

- `length_error` when the result is longer than 2³⁰ and the shorter longer than 48: the transform's length is past
  the plan's limit.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    // (1 + 2x + 3x²)(1 + x) = 1 + 3x + 5x² + 3x³
    vector<double> product = math::convolve(vector<double>{1, 2, 3}, vector<double>{1, 1});
    println(product);
    // a moving average of three over a step
    vector<double> step = {0, 0, 3, 3, 3, 3};
    vector<double> smooth = math::convolve(step, vector<double>{1.0 / 3, 1.0 / 3, 1.0 / 3});
    println("{:.1f} {:.1f} {:.1f} {:.1f}", smooth[2], smooth[3], smooth[4], smooth[5]);
}
```

Output:

```text
[1, 3, 5, 3]
1.0 2.0 3.0 3.0
```

## See also

- [fft](fft/README.md): the transforms
- [README: math](README.md)

[sgcl](../../README.md) › [concurrent](../README.md) › [hyperloglog](README.md)

# sgcl::concurrent::hyperloglog::estimate

```cpp
double estimate() const noexcept;
```

Estimates the distinct keys added, by Ertl's improved estimator over the histogram of the registers: zero for an
empty sketch, close to exact for a few keys, within 1.04/√2^*p* (one standard deviation) of the truth for many. Under
other threads' adds it is an estimate of some moment during the call.

## Parameters

None.

## Return value

The estimate.

## Complexity

Linear in 2^*p*: the registers read once, eight at a time.

## Exceptions

None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"
#include <cmath>

using namespace sgcl;

int main() {
    concurrent::hyperloglog h(12);
    for (uint64_t i = 0; i < 100'000; ++i) {
        h.add(i);
    }
    println("within 5%: {}", std::abs(h.estimate() / 100'000 - 1) < 0.05);
}
```

Output:

```text
within 5%: true
```

## See also

- [add](add.md)
- [sgcl::concurrent::hyperloglog](README.md)

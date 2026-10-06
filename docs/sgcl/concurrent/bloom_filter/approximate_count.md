[sgcl](../../README.md) › [concurrent](../README.md) › [bloom_filter](README.md)

# sgcl::concurrent::bloom_filter::approximate_count

```cpp
double approximate_count() const noexcept;
```

Estimates the keys added from the bits set, *X*: −*m*/*k* ln(1 − *X*/*m*) (Swamidass and Baldi). Keys added twice
count once; infinity once every bit is set. *X* is counted with the processor's population count, 64 bytes at a time
on NEON. Under other threads' adds it is an estimate of some moment during the call.

## Parameters

None.

## Return value

The estimate.

## Complexity

Linear in *m*: every word read once.

## Exceptions

None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::bloom_filter f(10'000, 0.01);
    for (uint64_t i = 0; i < 5000; ++i) {
        f.add(i);
    }
    println("{:.0f}", f.approximate_count() / 100);  // in hundreds
}
```

Output:

```text
50
```

## See also

- [false_positive_rate](false_positive_rate.md)
- [sgcl::concurrent::bloom_filter](README.md)

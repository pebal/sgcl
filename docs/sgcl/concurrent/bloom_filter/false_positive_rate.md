[sgcl](../../README.md) › [concurrent](../README.md) › [bloom_filter](README.md)

# sgcl::concurrent::bloom_filter::false_positive_rate

```cpp
double false_positive_rate() const noexcept;
```

Returns the rate at which [contains](contains.md) says yes to a key never added, as the bits set make it now:
(*X*/*m*)^*k*. Zero for an empty filter; it reaches the rate the filter was made for at the expected count of keys,
and grows past it as more are added.

## Parameters

None.

## Return value

The rate, from 0 to 1.

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
    for (uint64_t i = 0; i < 10'000; ++i) {
        f.add(i);
    }
    println("{:.1f}%", f.false_positive_rate() * 100);
}
```

Output:

```text
1.0%
```

## See also

- [approximate_count](approximate_count.md)
- [sgcl::concurrent::bloom_filter](README.md)

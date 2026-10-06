[sgcl](../../README.md) › [concurrent](../README.md) › [count_min_sketch](README.md)

# sgcl::concurrent::count_min_sketch::estimate

```cpp
uint64_t estimate(std::string_view key) const noexcept;    // (1)
template<class Bytes>
uint64_t estimate(const Bytes& key) const noexcept;        // (2)
uint64_t estimate(uint64_t key) const noexcept;            // (3)
```

Returns how often the key was added: the least of its counters, never less than the truth, and more than it by ε·*N*
only with a probability under δ. A key never added may read above zero, by the counts of the keys it shares its
counters with. The keys are those of [add](add.md): (1) text, (2) bytes, which takes part only for a key that converts
to `slice<const byte>` and not to `std::string_view`, (3) a number.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key |

## Return value

The estimated count.

## Complexity

Constant: one hash, *d* counters read.

## Exceptions

None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::count_min_sketch c;
    for (uint64_t i = 0; i < 1000; ++i) {
        c.add(i % 10);
    }
    println("{} {}", c.estimate(3), c.estimate(12345) <= 2);
}
```

Output:

```text
100 true
```

## See also

- [add](add.md)
- [total](total.md): *N*
- [sgcl::concurrent::count_min_sketch](README.md)

[sgcl](../../README.md) › [hash](../README.md) › [xxh32](README.md)

# sgcl::hash::xxh32::xxh32

```cpp
xxh32() noexcept = default;                // (1)
explicit xxh32(uint32_t seed) noexcept;    // (2)
```

Makes a hasher of no bytes yet.

1. With the seed 0, XXH32 without a seed: its values are those of `xxh32::of(data)` and of `xxhsum -H0`.
2. With `seed`: its values are those of `xxh32::of(data, seed)`, in every run and on every machine.

## Parameters

| Parameter | Description |
|---|---|
| `seed` | the seed, a number; 0 is XXH32 without one |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    hash::xxh32 plain;
    hash::xxh32 seeded(42);
    plain.update("hello");
    seeded.update("hello");
    println("{:08x}", plain.value());
    println("{:08x}", seeded.value());
    println("{} {}", plain.value() == hash::xxh32::of("hello"), seeded.value() == hash::xxh32::of("hello", 42));
}
```

Output:

```text
fb0077f9
4d02c966
true true
```

## See also

- [of](../mixin/hasher/of.md): the hash in one call, with a seed or without
- [sgcl::hash::xxh32](README.md)

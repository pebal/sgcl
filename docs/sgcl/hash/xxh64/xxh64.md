[sgcl](../../README.md) › [hash](../README.md) › [xxh64](README.md)

# sgcl::hash::xxh64::xxh64

```cpp
xxh64() noexcept = default;                // (1)
explicit xxh64(uint64_t seed) noexcept;    // (2)
```

Makes a hasher of no bytes yet.

1. With the seed 0, XXH64 without a seed: its values are those of `xxh64::of(data)` and of `xxhsum -H1`.
2. With `seed`: its values are those of `xxh64::of(data, seed)`, in every run and on every machine.

## Parameters

| Parameter | Description |
|---|---|
| `seed` | the seed, a number; 0 is XXH64 without one |

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
    hash::xxh64 plain;
    hash::xxh64 seeded(42);
    plain.update("hello");
    seeded.update("hello");
    println("{:016x}", plain.value());
    println("{:016x}", seeded.value());
    println("{} {}", plain.value() == hash::xxh64::of("hello"), seeded.value() == hash::xxh64::of("hello", 42));
}
```

Output:

```text
26c7827d889f6da3
c3629e6318d53932
true true
```

## See also

- [of](../mixin/hasher/of.md): the hash in one call, with a seed or without
- [sgcl::hash::xxh64](README.md)

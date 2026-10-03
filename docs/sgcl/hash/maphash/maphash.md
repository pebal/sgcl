[sgcl](../../README.md) › [hash](../README.md) › [maphash](../maphash.md)

# sgcl::hash::maphash::maphash

```cpp
/*(1)*/ maphash() noexcept;
/*(2)*/ explicit maphash(uint64_t seed) noexcept;
```

Makes a hasher of no bytes yet.

1. With the process's seed: every `maphash` of the process agrees with every other, and with `maphash::of(data)`.
   Go's zero `maphash.Hash` takes a random seed of its own instead.
2. With `seed`: the same values in every run and every process, those of `maphash::of(data, seed)`, for a test.

## Parameters

| Parameter | Description |
|---|---|
| `seed` | the seed, a number |

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
    hash::maphash h;
    h.update("hello");
    println("{:016x}", h.value());
    println("{}", h.value() == hash::maphash::of("hello"));

    hash::maphash fixed(42);
    fixed.update("hello");
    println("{:016x}", fixed.value());
}
```

Sample output:

```text
8c48d265bc1fcba7
true
bafa072f07db7937
```

## See also

- [of](../mixin/hasher/of.md): the hash in one call, with the process's seed or a seed
- [sgcl::hash::maphash](../maphash.md)

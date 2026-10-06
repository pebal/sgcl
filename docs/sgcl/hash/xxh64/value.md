[sgcl](../../README.md) › [hash](../README.md) › [xxh64](README.md)

# sgcl::hash::xxh64::value

```cpp
uint64_t value() const noexcept;
```

The hash of the bytes hashed so far. It ends nothing: the hasher joins its lanes and takes the bytes still buffered
on a copy, and [update](update.md) may go on after it.

## Parameters

None.

## Return value

The hash, a `uint64_t`.

## Complexity

Constant: the bytes still buffered, fewer than a stripe, and the join of the lanes.

## Exceptions

None.

## Example

```cpp
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    hash::xxh64 h;
    h.update("hello");
    println("{:016x}", h.value());
    h.update(", world");
    println("{:016x}", h.value());
    println("{}", h.value() == hash::xxh64::of("hello, world"));
}
```

Output:

```text
26c7827d889f6da3
b33a384e6d1b1242
true
```

## See also

- [digest](digest.md): the same as bytes
- [of](../mixin/hasher/of.md): the hash of data in one call
- [sgcl::hash::xxh64](README.md)

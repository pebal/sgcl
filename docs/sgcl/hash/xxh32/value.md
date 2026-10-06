[sgcl](../../README.md) › [hash](../README.md) › [xxh32](README.md)

# sgcl::hash::xxh32::value

```cpp
uint32_t value() const noexcept;
```

The hash of the bytes hashed so far. It ends nothing: the hasher joins its lanes and takes the bytes still buffered
on a copy, and [update](update.md) may go on after it.

## Parameters

None.

## Return value

The hash, a `uint32_t`.

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
    hash::xxh32 h;
    h.update("hello");
    println("{:08x}", h.value());
    h.update(", world");
    println("{:08x}", h.value());
    println("{}", h.value() == hash::xxh32::of("hello, world"));
}
```

Output:

```text
fb0077f9
4fa5ffd7
true
```

## See also

- [digest](digest.md): the same as bytes
- [of](../mixin/hasher/of.md): the hash of data in one call
- [sgcl::hash::xxh32](README.md)

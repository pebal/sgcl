[sgcl](../../README.md) › [hash](../README.md) › [fnv64a](README.md)

# sgcl::hash::fnv64a::value

```cpp
uint64_t value() const noexcept;
```

The hash of the bytes hashed so far. It ends nothing: [update](update.md) may go on after it, and `value()` then
gives the hash of the longer input, as Go's `h.Sum64()` does. The value of nothing is the offset basis,
`0xcbf29ce484222325`.

## Parameters

None.

## Return value

The hash, a `uint64_t`.

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
    hash::fnv64a h;
    h.update("foo");
    println("{:016x}", h.value());
    h.update("bar");
    println("{:016x}", h.value());
}
```

Output:

```text
dcb27518fed9d577
85944171f73967e8
```

## See also

- [digest](digest.md): the same as bytes
- [of](../mixin/hasher/of.md): the hash of data in one call
- [sgcl::hash::fnv64a](README.md)

[sgcl](../../README.md) › [hash](../README.md) › [fnv32a](README.md)

# sgcl::hash::fnv32a::value

```cpp
uint32_t value() const noexcept;
```

The hash of the bytes hashed so far. It ends nothing: [update](update.md) may go on after it, and `value()` then
gives the hash of the longer input, as Go's `h.Sum32()` does. The value of nothing is the offset basis,
`0x811c9dc5`.

## Parameters

None.

## Return value

The hash, a `uint32_t`.

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
    hash::fnv32a h;
    h.update("foo");
    println("{:08x}", h.value());
    h.update("bar");
    println("{:08x}", h.value());
}
```

Output:

```text
a9f37ed7
bf9cf968
```

## See also

- [digest](digest.md): the same as bytes
- [of](../mixin/hasher/of.md): the hash of data in one call
- [sgcl::hash::fnv32a](README.md)

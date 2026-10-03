[sgcl](../../README.md) › [hash](../README.md) › [fnv32](../fnv32.md)

# sgcl::hash::fnv32::value

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
    hash::fnv32 h;
    h.update("foo");
    println("{:08x}", h.value());
    h.update("bar");
    println("{:08x}", h.value());
}
```

Output:

```text
408f5e13
31f0b262
```

## See also

- [digest](digest.md): the same as bytes
- [of](../mixin/hasher/of.md): the hash of data in one call
- [sgcl::hash::fnv32](../fnv32.md)

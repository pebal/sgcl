[sgcl](../../README.md) › [hash](../README.md) › [siphash](../siphash.md)

# sgcl::hash::siphash::value

```cpp
uint64_t value() const noexcept;
```

The hash of the bytes hashed so far, the 64-bit result as the paper writes it: Go's `h.Sum64()`. It ends nothing:
the hasher finishes a copy of its state, and [update](update.md) may go on after it.

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
    array<byte, 16> key = {};
    hash::siphash h(key);
    h.update("hello");
    println("{:016x}", h.value());
    h.update(", world");
    println("{:016x}", h.value());
    println("{}", h.value() == hash::siphash::of("hello, world", key));
}
```

Output:

```text
8cc15d5db2f752b9
e3d9c9adb76d41bc
true
```

## See also

- [digest](digest.md): the same as bytes
- [of](../mixin/hasher/of.md): the hash of data in one call
- [sgcl::hash::siphash](../siphash.md)

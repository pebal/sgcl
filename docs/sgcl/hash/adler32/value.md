[sgcl](../../README.md) › [hash](../README.md) › [adler32](README.md)

# sgcl::hash::adler32::value

```cpp
uint32_t value() const noexcept;
```

The checksum of the bytes hashed so far, `b` in the high half and `a` in the low. It ends nothing:
[update](update.md) may go on after it, and `value()` then gives the checksum of the longer input, as Go's
`h.Sum32()` does. The checksum of nothing is 1.

## Parameters

None.

## Return value

The checksum, a `uint32_t`.

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
    hash::adler32 h;
    println("{:08x}", h.value());
    h.update("Wiki");
    println("{:08x}", h.value());
    h.update("pedia");
    println("{:08x}", h.value());
}
```

Output:

```text
00000001
03da0195
11e60398
```

## See also

- [digest](digest.md): the same as bytes
- [of](../mixin/hasher/of.md): the checksum of data in one call
- [sgcl::hash::adler32](README.md)

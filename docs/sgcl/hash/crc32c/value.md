[sgcl](../../README.md) › [hash](../README.md) › [crc32c](README.md)

# sgcl::hash::crc32c::value

```cpp
uint32_t value() const noexcept;
```

The CRC of the bytes hashed so far. It ends nothing: [update](update.md) may go on after it, and `value()` then
gives the CRC of the longer input, as Go's `h.Sum32()` does. The CRC of nothing is 0.

## Parameters

None.

## Return value

The CRC, a `uint32_t`.

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
    hash::crc32c h;
    println("{:08x}", h.value());
    h.update("1234");
    println("{:08x}", h.value());
    h.update("56789");
    println("{:08x}", h.value());
}
```

Output:

```text
00000000
f63af4ee
e3069283
```

## See also

- [digest](digest.md): the same as bytes
- [of](../mixin/hasher/of.md): the CRC of data in one call
- [sgcl::hash::crc32c](README.md)

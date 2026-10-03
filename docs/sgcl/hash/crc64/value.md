[sgcl](../../README.md) › [hash](../README.md) › [crc64](README.md)

# sgcl::hash::crc64::value

```cpp
uint64_t value() const noexcept;
```

The CRC of the bytes hashed so far. It ends nothing: [update](update.md) may go on after it, and `value()` then
gives the CRC of the longer input, as Go's `h.Sum64()` does. The CRC of nothing is 0.

## Parameters

None.

## Return value

The CRC, a `uint64_t`.

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
    hash::crc64 h;
    println("{:016x}", h.value());
    h.update("1234");
    println("{:016x}", h.value());
    h.update("56789");
    println("{:016x}", h.value());
}
```

Output:

```text
0000000000000000
ce4e879366b8c328
995dc9bbdf1939fa
```

## See also

- [digest](digest.md): the same as bytes
- [of](../mixin/hasher/of.md): the CRC of data in one call
- [sgcl::hash::crc64](README.md)

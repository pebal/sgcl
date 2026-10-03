[sgcl](../../README.md) › [hash](../README.md) › [crc32](../crc32.md)

# sgcl::hash::crc32::crc32

```cpp
crc32() noexcept = default;
```

Makes a hasher of no bytes yet: its register all ones, so that [value](value.md) is 0, the CRC of nothing. Go's
`crc32.NewIEEE()`. A hasher that goes on from a CRC saved earlier is made by [resume](resume.md); a copy is a
branch, a hasher that goes on from the same bytes on its own.

## Parameters

None.

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
    hash::crc32 h;
    println("{:08x}", h.value());
    h.update("123456789");
    println("{:08x}", h.value());

    hash::crc32 branch = h;
    branch.update("0");
    println("{} {}", h.value() == hash::crc32::of("123456789"), branch.value() == hash::crc32::of("1234567890"));
}
```

Output:

```text
00000000
cbf43926
true true
```

## See also

- [resume](resume.md): a hasher going on from a saved CRC
- [sgcl::hash::crc32](../crc32.md)

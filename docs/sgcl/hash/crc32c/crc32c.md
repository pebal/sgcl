[sgcl](../../README.md) › [hash](../README.md) › [crc32c](README.md)

# sgcl::hash::crc32c::crc32c

```cpp
crc32c() noexcept = default;
```

Makes a hasher of no bytes yet: its register all ones, so that [value](value.md) is 0, the CRC of nothing. Go's
`crc32.New(crc32.MakeTable(crc32.Castagnoli))`. A hasher that goes on from a CRC saved earlier is made by
[resume](resume.md); a copy is a branch, a hasher that goes on from the same bytes on its own.

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
    hash::crc32c h;
    println("{:08x}", h.value());
    h.update("123456789");
    println("{:08x}", h.value());

    hash::crc32c branch = h;
    branch.update("0");
    println("{} {}", h.value() == hash::crc32c::of("123456789"), branch.value() == hash::crc32c::of("1234567890"));
}
```

Output:

```text
00000000
e3069283
true true
```

## See also

- [resume](resume.md): a hasher going on from a saved CRC
- [sgcl::hash::crc32c](README.md)

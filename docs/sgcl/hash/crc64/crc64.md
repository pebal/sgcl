[sgcl](../../README.md) › [hash](../README.md) › [crc64](../crc64.md)

# sgcl::hash::crc64::crc64

```cpp
crc64() noexcept = default;
```

Makes a hasher of no bytes yet: its register all ones, so that [value](value.md) is 0, the CRC of nothing. Go's
`crc64.New(crc64.MakeTable(crc64.ECMA))`. A hasher that goes on from a CRC saved earlier is made by
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
    hash::crc64 h;
    println("{:016x}", h.value());
    h.update("123456789");
    println("{:016x}", h.value());

    hash::crc64 branch = h;
    branch.update("0");
    println("{} {}", h.value() == hash::crc64::of("123456789"), branch.value() == hash::crc64::of("1234567890"));
}
```

Output:

```text
0000000000000000
995dc9bbdf1939fa
true true
```

## See also

- [resume](resume.md): a hasher going on from a saved CRC
- [sgcl::hash::crc64](../crc64.md)

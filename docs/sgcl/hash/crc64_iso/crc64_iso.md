[sgcl](../../README.md) › [hash](../README.md) › [crc64_iso](README.md)

# sgcl::hash::crc64_iso::crc64_iso

```cpp
crc64_iso() noexcept = default;
```

Makes a hasher of no bytes yet: its register all ones, so that [value](value.md) is 0, the CRC of nothing. Go's
`crc64.New(crc64.MakeTable(crc64.ISO))`. A hasher that goes on from a CRC saved earlier is made by
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
    hash::crc64_iso h;
    println("{:016x}", h.value());
    h.update("123456789");
    println("{:016x}", h.value());

    hash::crc64_iso branch = h;
    branch.update("0");
    println("{} {}", h.value() == hash::crc64_iso::of("123456789"), branch.value() == hash::crc64_iso::of("1234567890"));
}
```

Output:

```text
0000000000000000
b90956c775a41001
true true
```

## See also

- [resume](resume.md): a hasher going on from a saved CRC
- [sgcl::hash::crc64_iso](README.md)

[sgcl](../../README.md) › [encoding](../README.md) › [asn1](../asn1/README.md) › [bits](README.md)

# sgcl::encoding::asn1::bits::operator[]

```cpp
bool operator[](size_t index) const noexcept;
```

The bit at the place, from 0, the first the top bit of the first byte; `index` below `length` (a debug
build asserts it).

## Parameters

| Parameter | Description |
|---|---|
| `index` | the place |

## Return value

The bit.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 usage = encoding::asn1::bit_string(vector<byte>{byte(0x86)}, 7);
    encoding::asn1::bits b = *usage.as_bits();
    for (size_t i : range(b.length)) {
        print("{}", b[i] ? '1' : '0');
    }
    println();
}
```

Output:

```text
1000011
```

## See also

- [sgcl::encoding::asn1::bits](README.md)

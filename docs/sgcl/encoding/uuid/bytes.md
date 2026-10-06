[sgcl](../../README.md) › [encoding](../README.md) › [uuid](README.md)

# sgcl::encoding::uuid::bytes

```cpp
array<uint8_t, 16> bytes() const noexcept;
```

The sixteen bytes as they are written, the first the top of `time_low`: what a binary column holds; an
[array](../../core/array/README.md) of `uint8_t`, as [net::ip_address](../../net/ip_address/README.md) gives its
own.

## Parameters

None.

## Return value

The bytes.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::uuid id("017f22e2-79b0-7cc3-98c4-dc0c0c07398f");
    for (uint8_t b : id.bytes()) {
        print("{:02x}", b);
    }
    println();
}
```

Output:

```text
017f22e279b07cc398c4dc0c0c07398f
```

## See also

- [from_bytes](from_bytes.md): the other way
- [sgcl::encoding::uuid](README.md)

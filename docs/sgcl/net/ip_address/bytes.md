[sgcl](../../README.md) › [net](../README.md) › [ip_address](README.md)

# sgcl::net::ip_address::bytes

```cpp
array<uint8_t, 16> bytes() const noexcept;
```

The sixteen bytes of the address in network order; Go's `Addr.As16`. An IPv4 address gives the bytes of its
IPv4-mapped form, `::ffff:a.b.c.d` (the form a dual-stack socket sees), its four bytes the last four. The zone is not
among them.

## Parameters

None.

## Return value

The sixteen bytes; all zero for the empty address.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    for (const char* text : {"192.168.0.1", "2001:db8::ff"}) {
        auto b = net::ip_address(text).bytes();
        for (uint8_t x : b) {
            print("{:02x}", x);
        }
        println("");
    }
}
```

Output:

```text
00000000000000000000ffffc0a80001
20010db80000000000000000000000ff
```

## See also

- [v4](v4.md), [v6](v6.md): an address of its bytes
- [sgcl::net::ip_address](README.md)

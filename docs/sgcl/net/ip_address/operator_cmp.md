[sgcl](../../README.md) › [net](../README.md) › [ip_address](README.md)

# sgcl::net::ip_address::operator==, operator\<=\>

```cpp
bool operator==(const ip_address&) const noexcept = default;     // (1)
auto operator<=>(const ip_address&) const noexcept = default;    // (2)
```

Compare two addresses member by member: the kind (the empty address, then IPv4, then IPv6), the sixteen bytes, the
zone (none first). `!=`, `<`, `<=`, `>` and `>=` are made of these by the compiler. The order is Go's `Addr.Compare`.

1. `true` when the kind, the bytes and the zone are the same. An IPv4 address and its IPv4-mapped form differ, as in
   Go.
2. The order of the two.

## Parameters

| Parameter | Description |
|---|---|
| the operand | the address compared with `*this`, unnamed in the declarations |

## Return value

- (1) Whether the addresses are equal.
- (2) `std::strong_ordering::less`, `equal` or `greater`.

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
    println("{}", net::ip_address::v4(1, 2, 3, 4) == net::ip_address("::ffff:1.2.3.4"));
    println("{}", net::ip_address("10.0.0.2") < net::ip_address("10.0.0.10"));
    println("{}", net::ip_address("255.255.255.255") < net::ip_address("::"));  // IPv4 first
    println("{}", net::ip_address("fe80::1") < net::ip_address("fe80::1%en0"));  // no zone first
}
```

Output:

```text
false
true
true
true
```

## See also

- [unmap](unmap.md): an IPv4-mapped address as IPv4
- [sgcl::net::ip_address](README.md)

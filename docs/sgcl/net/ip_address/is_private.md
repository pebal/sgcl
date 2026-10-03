[sgcl](../../README.md) › [net](../README.md) › [ip_address](../ip_address.md)

# sgcl::net::ip_address::is_private

```cpp
bool is_private() const noexcept;
```

Checks whether the address is private: RFC 1918 for IPv4 (`10.0.0.0/8`, `172.16.0.0/12`, `192.168.0.0/16`), RFC 4193
for IPv6 (`fc00::/7`); Go's `Addr.IsPrivate`. It looks through the mapping, so `::ffff:a.b.c.d` answers as `a.b.c.d`
does, and ignores the zone.

## Parameters

None.

## Return value

`true` when the address is in the ranges named, `false` otherwise and for the empty address.

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
    for (const char* text : {"10.1.2.3", "172.31.0.1", "172.32.0.1", "192.168.0.1", "fd00::1", "::ffff:10.1.2.3", "8.8.8.8"}) {
        println("{} {}", text, net::ip_address(text).is_private());
    }
}
```

Output:

```text
10.1.2.3 true
172.31.0.1 true
172.32.0.1 false
192.168.0.1 true
fd00::1 true
::ffff:10.1.2.3 true
8.8.8.8 false
```

## See also

- [is_loopback](is_loopback.md), [is_unspecified](is_unspecified.md), [is_multicast](is_multicast.md),
  [is_link_local](is_link_local.md), [is_global_unicast](is_global_unicast.md): the other predicates
- [unmap](unmap.md): an IPv4-mapped address as IPv4
- [sgcl::net::ip_address](../ip_address.md)

[sgcl](../../README.md) › [net](../README.md) › [ip_address](README.md)

# sgcl::net::ip_address::is_link_local

```cpp
bool is_link_local() const noexcept;
```

Checks whether the address is link-local unicast, `169.254.0.0/16` or `fe80::/10`; Go's `Addr.IsLinkLocalUnicast`. The
link-local multicast addresses (`224.0.0.0/24`, `ff02::/16`) are multicast, not this. It looks through the mapping, so
`::ffff:a.b.c.d` answers as `a.b.c.d` does, and ignores the zone.

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
    for (const char* text : {"169.254.1.1", "fe80::1%en0", "febf::1", "fec0::1", "ff02::1"}) {
        println("{} {}", text, net::ip_address(text).is_link_local());
    }
}
```

Output:

```text
169.254.1.1 true
fe80::1%en0 true
febf::1 true
fec0::1 false
ff02::1 false
```

## See also

- [is_loopback](is_loopback.md), [is_private](is_private.md), [is_unspecified](is_unspecified.md),
  [is_multicast](is_multicast.md), [is_global_unicast](is_global_unicast.md): the other predicates
- [unmap](unmap.md): an IPv4-mapped address as IPv4
- [sgcl::net::ip_address](README.md)

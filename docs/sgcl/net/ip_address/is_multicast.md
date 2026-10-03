[sgcl](../../README.md) › [net](../README.md) › [ip_address](README.md)

# sgcl::net::ip_address::is_multicast

```cpp
bool is_multicast() const noexcept;
```

Checks whether the address is multicast, `224.0.0.0/4` or `ff00::/8`; Go's `Addr.IsMulticast`. It looks through the
mapping, so `::ffff:a.b.c.d` answers as `a.b.c.d` does, and ignores the zone.

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
    for (const char* text : {"224.0.0.251", "239.255.255.250", "ff02::1", "::ffff:224.0.0.1", "192.168.0.1"}) {
        println("{} {}", text, net::ip_address(text).is_multicast());
    }
}
```

Output:

```text
224.0.0.251 true
239.255.255.250 true
ff02::1 true
::ffff:224.0.0.1 true
192.168.0.1 false
```

## See also

- [is_loopback](is_loopback.md), [is_private](is_private.md), [is_unspecified](is_unspecified.md),
  [is_link_local](is_link_local.md), [is_global_unicast](is_global_unicast.md): the other predicates
- [unmap](unmap.md): an IPv4-mapped address as IPv4
- [sgcl::net::ip_address](README.md)

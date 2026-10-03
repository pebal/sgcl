[sgcl](../../README.md) › [net](../README.md) › [ip_address](README.md)

# sgcl::net::ip_address::is_global_unicast

```cpp
bool is_global_unicast() const noexcept;
```

Checks whether the address is global unicast, Go's `Addr.IsGlobalUnicast`: a valid address that is neither
unspecified, nor loopback, multicast, link-local unicast, nor the IPv4 broadcast address `255.255.255.255`. A private
address is global unicast: the name is the RFC's, not a statement about routing. It looks through the mapping, so
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
    for (const char* text : {"8.8.8.8", "10.0.0.1", "2001:db8::1", "255.255.255.255", "127.0.0.1", "fe80::1", "0.0.0.0"}) {
        println("{} {}", text, net::ip_address(text).is_global_unicast());
    }
}
```

Output:

```text
8.8.8.8 true
10.0.0.1 true
2001:db8::1 true
255.255.255.255 false
127.0.0.1 false
fe80::1 false
0.0.0.0 false
```

## See also

- [is_loopback](is_loopback.md), [is_private](is_private.md), [is_unspecified](is_unspecified.md),
  [is_multicast](is_multicast.md), [is_link_local](is_link_local.md): the other predicates
- [unmap](unmap.md): an IPv4-mapped address as IPv4
- [sgcl::net::ip_address](README.md)

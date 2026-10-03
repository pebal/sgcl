[sgcl](../../README.md) › [net](../README.md) › [ip_address](../ip_address.md)

# sgcl::net::ip_address::is_loopback

```cpp
bool is_loopback() const noexcept;
```

Checks whether the address is a loopback address, `127.0.0.0/8` or `::1`; Go's `Addr.IsLoopback`. It looks through the
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
    for (const char* text : {"127.0.0.1", "127.255.0.9", "::1", "::ffff:127.0.0.1", "10.0.0.1"}) {
        println("{} {}", text, net::ip_address(text).is_loopback());
    }
}
```

Output:

```text
127.0.0.1 true
127.255.0.9 true
::1 true
::ffff:127.0.0.1 true
10.0.0.1 false
```

## See also

- [is_private](is_private.md), [is_unspecified](is_unspecified.md), [is_multicast](is_multicast.md),
  [is_link_local](is_link_local.md), [is_global_unicast](is_global_unicast.md): the other predicates
- [unmap](unmap.md): an IPv4-mapped address as IPv4
- [sgcl::net::ip_address](../ip_address.md)

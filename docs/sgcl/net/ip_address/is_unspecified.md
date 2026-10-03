[sgcl](../../README.md) › [net](../README.md) › [ip_address](../ip_address.md)

# sgcl::net::ip_address::is_unspecified

```cpp
bool is_unspecified() const noexcept;
```

Checks whether the address is the unspecified one, `0.0.0.0` or `::`; Go's `Addr.IsUnspecified`. This predicate does
not look through the mapping: `::ffff:0.0.0.0` is not unspecified. It ignores the zone: `::%en0` is unspecified here,
where Go, which compares the zone too, says it is not.

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
    for (const char* text : {"0.0.0.0", "::", "::%en0", "::ffff:0.0.0.0", "127.0.0.1"}) {
        println("{} {}", text, net::ip_address(text).is_unspecified());
    }
}
```

Output:

```text
0.0.0.0 true
:: true
::%en0 true
::ffff:0.0.0.0 false
127.0.0.1 false
```

## See also

- [is_loopback](is_loopback.md), [is_private](is_private.md), [is_multicast](is_multicast.md),
  [is_link_local](is_link_local.md), [is_global_unicast](is_global_unicast.md): the other predicates
- [unmap](unmap.md): an IPv4-mapped address as IPv4
- [sgcl::net::ip_address](../ip_address.md)

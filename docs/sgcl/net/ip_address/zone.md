[sgcl](../../README.md) › [net](../README.md) › [ip_address](README.md)

# sgcl::net::ip_address::zone

```cpp
string zone() const noexcept;
```

The zone of a scoped IPv6 address, the interface it is on: `en0` of `fe80::1%en0`; Go's `Addr.Zone`. A numeric zone
(`fe80::1%4`) is an interface index; one past the largest index names no interface, and a dial of it is
`net::errc::invalid_address`, never another interface by wrap-around.

## Parameters

None.

## Return value

The zone, at most fifteen bytes, or the empty string for an address without one.

## Complexity

Linear in the length of the zone.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::ip_address scoped("fe80::1%en0");
    println("[{}] [{}]", scoped.zone(), net::ip_address("fe80::1").zone());
}
```

Output:

```text
[en0] []
```

## See also

- [has_zone](has_zone.md): whether there is one
- [with_zone](with_zone.md): the address with another
- [sgcl::net::ip_address](README.md)

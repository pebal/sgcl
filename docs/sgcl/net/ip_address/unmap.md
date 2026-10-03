[sgcl](../../README.md) › [net](../README.md) › [ip_address](README.md)

# sgcl::net::ip_address::unmap

```cpp
ip_address unmap() const noexcept;
```

The IPv4 address of an IPv4-mapped one: `::ffff:1.2.3.4` becomes `1.2.3.4`; any other address is returned unchanged.
Go's `Addr.Unmap`.

An IPv4 address and its mapped form are two values, as in Go: they differ under `==`, the order and the hash, and an
[ip_network](../ip_network/README.md) of IPv4 does not contain the mapped one. `unmap()` first where the mapping should not
matter, such as the peer of a dual-stack listener.

## Parameters

None.

## Return value

The IPv4 address, or `*this`.

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
    net::ip_address peer("::ffff:192.168.0.7");
    println("{} {}", peer.unmap(), peer == net::ip_address("192.168.0.7"));
    println("{}", peer.unmap() == net::ip_address("192.168.0.7"));
    println(net::ip_address("2001:db8::1").unmap().to_string());
}
```

Output:

```text
192.168.0.7 false
true
2001:db8::1
```

## See also

- [is_v4_mapped](is_v4_mapped.md): whether there is anything to unmap
- [sgcl::net::ip_address](README.md)

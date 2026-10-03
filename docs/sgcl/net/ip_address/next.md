[sgcl](../../README.md) › [net](../README.md) › [ip_address](README.md)

# sgcl::net::ip_address::next

```cpp
ip_address next() const noexcept;
```

The address one above, within the kind, the zone kept; Go's `Addr.Next`. Past the last address of the kind
(`255.255.255.255` for IPv4, `ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff` for IPv6) there is none, and the result is the
empty address; so it is for the empty address.

## Parameters

None.

## Return value

The address one above, or the empty address.

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
    println("{}", net::ip_address::v4(192, 168, 1, 255).next());
    println(net::ip_address("fe80::ffff%en0").next().to_string());
    println(net::ip_address("255.255.255.255").next().to_string());
}
```

Output:

```text
192.168.2.0
fe80::1:0%en0
invalid IP
```

## See also

- [prev](prev.md): the other way
- [ip_network::masked](../ip_network/masked.md): the first address of a network
- [sgcl::net::ip_address](README.md)

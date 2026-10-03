[sgcl](../../README.md) › [net](../README.md) › [ip_address](README.md)

# sgcl::net::ip_address::prev

```cpp
ip_address prev() const noexcept;
```

The address one below, within the kind, the zone kept; Go's `Addr.Prev`. Below the first address of the kind
(`0.0.0.0` for IPv4, `::` for IPv6) there is none, and the result is the empty address; so it is for the empty
address.

## Parameters

None.

## Return value

The address one below, or the empty address.

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
    println("{}", net::ip_address::v4(192, 168, 2, 0).prev());
    println(net::ip_address("fe80::1:0%en0").prev().to_string());
    println(net::ip_address("::").prev().to_string());
}
```

Output:

```text
192.168.1.255
fe80::ffff%en0
invalid IP
```

## See also

- [next](next.md): the other way
- [ip_network::masked](../ip_network/masked.md): the first address of a network
- [sgcl::net::ip_address](README.md)

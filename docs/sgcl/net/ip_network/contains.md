[sgcl](../../README.md) › [net](../README.md) › [ip_network](../ip_network.md)

# sgcl::net::ip_network::contains

```cpp
bool contains(const ip_address& a) const noexcept;
```

Checks whether the address `a` lies in the network, Go's `Prefix.Contains`: the address is of the network's kind, has
no zone, and its first `bits()` bits are the network's.

The question is asked of the value as it is, as Go asks it: an IPv4-mapped address is not in an IPv4 network, nor an
address with a zone in any. [unmap](../ip_address/unmap.md) first, and [with_zone](../ip_address/with_zone.md) of
`""`, when they should not matter.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the address |

## Return value

`true` when `a` lies in the network; `false` otherwise, and when either is empty.

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
    net::ip_network lan("192.168.0.0/16");
    println("{}", lan.contains(net::ip_address("192.168.7.9")));
    println("{}", lan.contains(net::ip_address("192.169.0.1")));

    net::ip_address mapped("::ffff:192.168.7.9");
    println("{} {}", lan.contains(mapped), lan.contains(mapped.unmap()));
    println("{}", net::ip_network("fe80::/10").contains(net::ip_address("fe80::1%en0")));
}
```

Output:

```text
true
false
false true
false
```

## See also

- [overlaps](overlaps.md): two networks
- [masked](masked.md): the first address
- [sgcl::net::ip_network](../ip_network.md)

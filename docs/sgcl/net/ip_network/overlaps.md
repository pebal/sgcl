[sgcl](../../README.md) › [net](../README.md) › [ip_network](README.md)

# sgcl::net::ip_network::overlaps

```cpp
bool overlaps(const ip_network& o) const noexcept;
```

Checks whether the two networks have an address in common, Go's `Prefix.Overlaps`: they are of one kind, and their
addresses agree on the shorter of the two prefixes. A network of IPv4 and one of IPv6 never overlap.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the other network |

## Return value

`true` when the networks share an address; `false` otherwise, and when either is empty.

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
    println("{}", lan.overlaps(net::ip_network("192.168.4.0/22")));
    println("{}", lan.overlaps(net::ip_network("192.0.0.0/8")));
    println("{}", lan.overlaps(net::ip_network("10.0.0.0/8")));
    println("{}", net::ip_network("0.0.0.0/0").overlaps(net::ip_network("::/0")));
}
```

Output:

```text
true
true
false
false
```

## See also

- [contains](contains.md): one address
- [sgcl::net::ip_network](README.md)

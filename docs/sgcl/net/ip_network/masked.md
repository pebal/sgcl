[sgcl](../../README.md) › [net](../README.md) › [ip_network](../ip_network.md)

# sgcl::net::ip_network::masked

```cpp
ip_network masked() const noexcept;
```

The same network with the bits of the address past the prefix cleared, Go's `Prefix.Masked`: `10.1.2.3/8` becomes
`10.0.0.0/8`. The address of the result is the first address of the network.

## Parameters

None.

## Return value

The masked network; the empty network for the empty one.

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
    println(net::ip_network("10.1.2.3/8").masked().to_string());
    println(net::ip_network("192.168.77.200/20").masked().to_string());
    println(net::ip_network("2001:db8:abcd:12::1/48").masked().to_string());
}
```

Output:

```text
10.0.0.0/8
192.168.64.0/20
2001:db8:abcd::/48
```

## See also

- [address](address.md): the address as given
- [contains](contains.md): whether an address lies in the network
- [sgcl::net::ip_network](../ip_network.md)

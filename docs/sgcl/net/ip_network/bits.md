[sgcl](../../README.md) › [net](../README.md) › [ip_network](../ip_network.md)

# sgcl::net::ip_network::bits

```cpp
int bits() const noexcept;
```

The length of the prefix in bits, Go's `Prefix.Bits`.

## Parameters

None.

## Return value

0 to 32 for IPv4, 0 to 128 for IPv6; -1 for the empty network.

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
    println("{} {} {}", net::ip_network("10.0.0.0/8").bits(), net::ip_network("2001:db8::/32").bits(),
            net::ip_network().bits());
}
```

Output:

```text
8 32 -1
```

## See also

- [address](address.md): the address
- [sgcl::net::ip_network](../ip_network.md)

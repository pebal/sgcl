[sgcl](../../README.md) › [net](../README.md) › [ip_network](README.md)

# sgcl::net::ip_network::address

```cpp
ip_address address() const noexcept;
```

The address of the network as it was given, Go's `Prefix.Addr`: `10.1.2.3` of `10.1.2.3/8`. The first address of the
network is `masked().address()`.

## Parameters

None.

## Return value

The address, without a zone; the empty address for the empty network.

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
    net::ip_network n("10.1.2.3/8");
    println("{} {}", n.address(), n.masked().address());
}
```

Output:

```text
10.1.2.3 10.0.0.0
```

## See also

- [bits](bits.md): the length
- [masked](masked.md): the bits past the prefix cleared
- [sgcl::net::ip_network](README.md)

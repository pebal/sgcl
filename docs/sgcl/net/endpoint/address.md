[sgcl](../../README.md) › [net](../README.md) › [endpoint](README.md)

# sgcl::net::endpoint::address

```cpp
ip_address address() const noexcept;
```

The address of the endpoint, with its zone; Go's `AddrPort.Addr`.

## Parameters

None.

## Return value

The address; the empty address for the empty endpoint.

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
    net::endpoint e("[fe80::1%en0]:22");
    println("{} {}", e.address(), e.address().is_link_local());
}
```

Output:

```text
fe80::1%en0 true
```

## See also

- [port](port.md): the port
- [sgcl::net::endpoint](README.md)

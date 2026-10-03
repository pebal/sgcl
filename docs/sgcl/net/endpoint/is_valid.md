[sgcl](../../README.md) › [net](../README.md) › [endpoint](../endpoint.md)

# sgcl::net::endpoint::is_valid

```cpp
bool is_valid() const noexcept;
```

Checks whether the endpoint has an address, Go's `AddrPort.IsValid`; the port does not matter, 0 included.

## Parameters

None.

## Return value

`true` when the address is valid, `false` for the empty endpoint.

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
    net::endpoint none;
    println("{} {}", none.is_valid(), net::endpoint(net::ip_address::loopback_v4(), 0).is_valid());
}
```

Output:

```text
false true
```

## See also

- [address](address.md): the address
- [sgcl::net::endpoint](../endpoint.md)

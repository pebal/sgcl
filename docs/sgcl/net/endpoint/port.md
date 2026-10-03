[sgcl](../../README.md) › [net](../README.md) › [endpoint](../endpoint.md)

# sgcl::net::endpoint::port

```cpp
uint16_t port() const noexcept;
```

The port of the endpoint, Go's `AddrPort.Port`.

## Parameters

None.

## Return value

The port, 0 to 65535; 0 for the empty endpoint.

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
    println("{} {}", net::endpoint("10.0.0.1:0443").port(), net::endpoint().port());
}
```

Output:

```text
443 0
```

## See also

- [address](address.md): the address
- [sgcl::net::endpoint](../endpoint.md)

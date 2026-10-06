[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [client](README.md)

# sgcl::net::nats::client::max_payload

```cpp
size_t max_payload() const noexcept;
```

The largest message the server takes, from its INFO; a larger publication is refused by the client.

## Parameters

None.

## Return value

The size in bytes.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/nats.h"

using namespace sgcl;

int main() {
    net::nats::client nc = net::nats::client::connect("nats://localhost:4222").value();
    println("{}", nc.max_payload());
}
```

Output:

```text
1048576
```

## See also

- [publish](publish.md)
- [client](README.md)

[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [client](README.md)

# sgcl::net::nats::client::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle holds a connection; an ended connection is still held.

## Parameters

None.

## Return value

Whether it does.

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
    net::nats::client none;
    println("{} {}", bool(none), bool(nc));
}
```

Output:

```text
false true
```

## See also

- [(constructor)](client.md)
- [client](README.md)

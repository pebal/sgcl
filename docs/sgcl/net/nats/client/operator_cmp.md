[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [client](README.md)

# sgcl::net::nats::operator== (sgcl::net::nats::client)

```cpp
friend bool operator==(const client& a, const client& b) noexcept;
```

Whether two handles are the same connection; `!=` is its negation.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles |

## Return value

Whether they are the same connection.

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
    net::nats::client same = nc;
    net::nats::client other = net::nats::client::connect("nats://localhost:4222").value();
    println("{} {}", same == nc, other == nc);
}
```

Output:

```text
true false
```

## See also

- [client](README.md)

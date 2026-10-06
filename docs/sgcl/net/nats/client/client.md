[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [client](README.md)

# sgcl::net::nats::client::client

```cpp
client() noexcept;                       // (1)
client(const client& other) noexcept;    // (2)
```

1. No connection: `operator bool` is false; an operation on it is a contract violation.
2. The same connection as `other`.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle copied |


## Return value

None.

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
    net::nats::client same = nc;
    println("{} {}", bool(none), same == nc);
}
```

Output:

```text
false true
```

## See also

- [connect](connect.md)
- [client](README.md)

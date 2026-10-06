[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [client](README.md)

# sgcl::net::nats::client::server_id

```cpp
string server_id() const;
```

The server's id, from its INFO.

## Parameters

None.

## Return value

The id.

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
    println("{}", nc.server_id());
}
```

Output:

```text
NTEST
```

## See also

- [max_payload](max_payload.md)
- [client](README.md)

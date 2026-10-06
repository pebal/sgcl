[sgcl](../../../README.md) › [net](../../README.md) › [pop3](../README.md) › [server](README.md)

# sgcl::net::pop3::server::server

```cpp
server() noexcept;                        // (1)
server(const server& other) = default;    // (2)
```

1. A server with a memory_backend of its own, no TLS, the defaults of every field.
2. The same server: the handle copied, the connections shared.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the server to share |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/imap.h"
#include "sgcl/net/pop3.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::pop3::server srv;
    net::pop3::server copy = srv;
    println("{} {}", srv.connections(), copy.greeting);
}
```

Output:

```text
0 POP3 server ready
```

## See also

- [serve](serve.md)
- [server](README.md)

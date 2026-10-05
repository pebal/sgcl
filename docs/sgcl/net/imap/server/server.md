[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [server](README.md)

# sgcl::net::imap::server::server

```cpp
server();                                 // (1)
server(const server& other) = default;    // (2)
```

1. Constructs a server over a [memory_backend](../memory_backend/README.md) of its own, its fields at their
   defaults, no connection.
2. The same server as `other` (its connections and open mailboxes), the fields copied. A move is the copy.

## Parameters

| Parameter | Description |
|---|---|
| `other` | another server |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    net::imap::server srv;
    net::imap::server same = srv;
    println("{} {}", srv.connections(), same.connections());
}
```

Output:

```text
0 0
```

## See also

- [serve](serve.md)
- [sgcl::net::imap::server](README.md)

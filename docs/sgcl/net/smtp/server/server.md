[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [server](README.md)

# sgcl::net::smtp::server::server

```cpp
server() noexcept;                       // (1)
server(const server& other) noexcept;    // (2)
```

1. A server with no handler (a message is taken and dropped) and the fields' defaults.
2. A handle of `other`'s server: the handler and the connections shared, the fields copied.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the server whose handle is copied |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::smtp::server srv;
    net::smtp::server copy = srv;
    copy.close();
    println("{}", srv.serve("127.0.0.1:0").error().code() == net::errc::server_closed);
}
```

Output:

```text
true
```

## See also

- [server](README.md)

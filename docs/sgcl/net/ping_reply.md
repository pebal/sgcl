[sgcl](../README.md) › [net](README.md)

# sgcl::net::ping_reply

```cpp
#include "sgcl/net/ping.h"

namespace sgcl::net {
    struct ping_reply {
        ip_address from;
        duration rtt;
        size_t bytes = 0;
        int ttl = -1;
        uint16_t sequence = 0;
    };
}
```

`sgcl::net::ping_reply` is an echo's reply ([ping](ping.md)).

## Member objects

| Member | Description |
|---|---|
| `from` | who answered |
| `rtt` | the round trip |
| `bytes` | the ICMP message's size, its head included |
| `ttl` | the TTL the reply arrived with; -1 where the socket does not tell (IPv6) |
| `sequence` | the echo's sequence number |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ping.h"

using namespace sgcl;

int main() {
    auto r = net::ping("localhost").value();
    println("{} {}", r.from.is_loopback(), r.bytes);
}
```

Output:

```text
true 64
```

## See also

- [ping](ping.md)

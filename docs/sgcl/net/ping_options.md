[sgcl](../README.md) › [net](README.md)

# sgcl::net::ping_options

```cpp
#include "sgcl/net/ping.h"

namespace sgcl::net {
    struct ping_options {
        duration timeout = std::chrono::seconds(2);
        size_t size = 56;
        int ttl = 0;
        async::stop_token stop;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::ping_options` is how a [ping](ping.md) is asked.

## Member objects

| Member | Description |
|---|---|
| `timeout` | the reply waited for at most; 2 s by default; zero: none |
| `size` | the payload's bytes, 8 at least; 56 by default (64 with the ICMP head, as ping's) |
| `ttl` | the request's TTL (hop limit); 0 by default: the system's |
| `stop` | ends the wait with `ECANCELED` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ping.h"

using namespace sgcl;

int main() {
    net::ping_options o;
    o.size = 1000;
    println("{}", net::ping("127.0.0.1", o)->bytes);
}
```

Output:

```text
1008
```

## See also

- [ping](ping.md)

[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [reverse_proxy](reverse_proxy/README.md) › balancing

# sgcl::net::http::reverse_proxy::balancing

```cpp
#include "sgcl/net/http/reverse_proxy.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class reverse_proxy {
    public:
        enum class balancing : uint8_t {
            round_robin,
            least_connections,
            random,
        };
    };
}
```

How a [reverse_proxy](reverse_proxy/README.md) of several backends chooses the backend of a request, among those in
the turn: passing their health checks and not skipped after failures. When none is in the turn, every one is (the
proxy fails open: a backend that may answer beats a 502 that will); a retry chooses among those the request has not
tried yet.

| Value | Description |
|---|---|
| `round_robin` | in turn, the default |
| `least_connections` | the one with the fewest requests in progress, in turn among equals: for requests of very different lengths |
| `random` | at random, uniformly |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    auto name = [](const char* n) {
        return [n](net::http::request, net::http::response_writer w) { w.write(n); };
    };
    net::http::test_server a(name("a")), b(name("b")), c(name("c"));
    net::http::reverse_proxy::options o;
    o.policy = net::http::reverse_proxy::balancing::round_robin;
    net::http::reverse_proxy proxy({a.url(), b.url(), c.url()}, o);
    string order;
    for (int i : range(6)) {
        net::http::response_recorder rec;
        proxy(net::http::test_request("GET", "/"), rec.writer()).wait();
        order = order + rec.body();
    }
    println("{}", order);
}
```

Output:

```text
abcabc
```

## See also

- [reverse_proxy::options](reverse_proxy-options.md): `policy`
- [reverse_proxy](reverse_proxy/README.md)

[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [reverse_proxy](reverse_proxy/README.md) › backend

# sgcl::net::http::reverse_proxy::backend

```cpp
#include "sgcl/net/http/reverse_proxy.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class reverse_proxy {
    public:
        struct backend {
            string url;
            bool healthy = true;
            int64_t active = 0;
            uint64_t requests = 0;
            uint64_t failures = 0;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::reverse_proxy::backend` is a backend of a [reverse_proxy](reverse_proxy/README.md) and its counts,
as [backends](reverse_proxy/backends.md) reads them: a plain struct, a snapshot taken field by field.

## Member objects

| Member | Description |
|---|---|
| `url` | the URL as the constructor was given it |
| `healthy` | in the turn: passing the health checks (when there are any) and not skipped after `max_fails` failures in a row |
| `active` | the requests in progress with it, from the send to the end of the response's body |
| `requests` | the requests sent to it, the ones that failed among them |
| `failures` | the sends that failed: no connection, or no head before an error or a timeout |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::test_server live([](net::http::request, net::http::response_writer w) {
        w.write("ok");
    });
    net::http::reverse_proxy::options o;
    o.max_fails = 1;  // skipped after one failure
    net::http::reverse_proxy proxy({"http://127.0.0.1:1", live.url()}, o);
    net::http::response_recorder rec;
    proxy(net::http::test_request("GET", "/"), rec.writer()).wait();
    println("{} {}", rec.status(), rec.body());
    auto dead = proxy.backends()[0];
    println("{} | in the turn: {}, {} sent, {} failed", dead.url, dead.healthy, dead.requests,
            dead.failures);
    println("the other: {} sent", proxy.backends()[1].requests);
}
```

Output:

```text
200 ok
http://127.0.0.1:1 | in the turn: false, 1 sent, 1 failed
the other: 1 sent
```

## See also

- [backends](reverse_proxy/backends.md)
- [reverse_proxy](reverse_proxy/README.md)

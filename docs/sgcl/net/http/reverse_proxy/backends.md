[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [reverse_proxy](README.md)

# sgcl::net::http::reverse_proxy::backends

```cpp
vector<backend> backends() const;
```

Returns the backends in the order given, each with its counts ([backend](../reverse_proxy-backend.md)): whether it
is in the turn (passing its health checks and not skipped after failures), the requests it has in progress, the
requests sent to it, the sends that failed. The counts are read without a lock, each by itself: a snapshot that a
request in flight may change.

## Parameters

None.

## Return value

The backends and their counts.

## Complexity

Linear in the number of backends.

## Exceptions

None.

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
    net::http::test_server a(name("a")), b(name("b"));
    net::http::reverse_proxy balanced({a.url(), b.url(), "http://127.0.0.1:1"});
    net::http::server front;
    front.route("/", balanced);
    net::http::test_server proxy(front);
    for (int i : range(6)) {
        proxy.client().get(proxy.url())->text();
    }
    for (auto& x : balanced.backends()) {
        println("{} requests, {} failed, in the turn: {}", x.requests, x.failures, x.healthy);
    }
}
```

Output:

```text
3 requests, 0 failed, in the turn: true
3 requests, 0 failed, in the turn: true
2 requests, 2 failed, in the turn: true
```

## See also

- [backend](../reverse_proxy-backend.md)
- [options](../reverse_proxy-options.md): `max_fails`, `health_path`
- [sgcl::net::http::reverse_proxy](README.md)

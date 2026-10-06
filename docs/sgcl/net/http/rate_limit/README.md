[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::rate_limit

```cpp
#include "sgcl/net/http/middleware.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class rate_limit;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

Requests per key, a [middleware](../middleware.md): a token bucket for each key — an
[async::rate_limiter](../../../async/rate_limiter/README.md) of `per_second` tokens a second, `burst` at once — and a
request whose key has no token now answered 429 Too Many Requests with `Retry-After`, the seconds until it would have
one (RFC 6585 §4). The key is the client's address by default, a field's value (an API key), or what a function of the
program's makes of the request. Go's standard library has none (x/time/rate is the bucket alone).

## Rules

- A bucket is made at a key's first request and dropped after `idle` without one (it was full again long before);
  past `max_keys` the idle buckets go first, then the least recently used. A key costs its bucket's few words.
- A request takes the token without waiting: a server that waits for tokens holds its connections.
- Behind a reverse proxy the client's address is the proxy's: the key is then a field the proxy sets
  (`X-Forwarded-For`), or the program's function.
- A handle of one word: copies share the buckets.

## Member types

| Type | Definition |
|---|---|
| [options](../rate_limit-options.md) | the key, the idle time, the most keys |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](rate_limit.md) | the middleware of a rate, a burst and the options |
| [keys](keys.md) | the buckets kept now |
| [wrap](wrap.md) | the middleware around one handler |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.use(net::http::rate_limit(1, 3));   // a request a second, three at once
    srv.route("GET /", [](net::http::request, net::http::response_writer w) { w.write("ok"); });
    for (int i : range(5)) {
        net::http::response_recorder rec;
        rec.serve(srv, net::http::test_request("GET", "/"));
        string retry = rec.header("Retry-After");
        println("{}{}", rec.status(), retry.empty() ? string() : " retry after " + retry + " s");
    }
}
```

Output:

```text
200
200
200
429 retry after 1 s
429 retry after 1 s
```

## See also

- [async::rate_limiter](../../../async/rate_limiter/README.md)
- [rate_limit::options](../rate_limit-options.md)
- [middleware](../middleware.md)
- [sgcl::net::http](../README.md)

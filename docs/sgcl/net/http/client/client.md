[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [client](../client.md)

# sgcl::net::http::client::client

```cpp
client() noexcept;
```

Constructs a client with an empty pool of its own and the default settings: no total timeout, 30 s to connect, no
bound on the wait for the head, 90 s for an idle connection, 16 idle connections an origin, 10 redirects, 1 MB of a
response's head, the dial of [tcp::connect](../../tcp/connect.md), the default TLS config with ALPN `http/1.1`,
HTTP/2 offered over TLS and no `h2c`. Nothing is dialed until the first request. The settings are fields, changed
after the construction, and each request reads them when it starts.

A copy, made by the copy constructor, shares the pool and carries its own settings: a copy with a shorter `timeout`
asks over the same connections. There is no move of its own: a move copies, so a moved-from client is the same
client, its dial and TLS settings kept, as a moved-from [tracked_ptr](../../../core/tracked_ptr.md) still points.

## Parameters

None.

## Complexity

Constant: the pool is one managed object.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    net::http::client web;
    println("{} {} {} {}", web.connect_timeout == 30s, web.idle_timeout == 90s,
            web.max_idle_per_host, web.max_redirects);
    println("{} {} {}", web.tls.alpn, web.http2, web.h2c);

    net::http::client hasty = web;  // the same pool, settings of its own
    hasty.timeout = 2s;
    println("{} {}", web.timeout == 0s, hasty.timeout == 2s);
}
```

Output:

```text
true true 16 10
["http/1.1"] true false
true true
```

## See also

- [send](send.md): what the settings bound
- [sgcl::net::http::client](../client.md)

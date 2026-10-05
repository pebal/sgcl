[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [proxy](README.md)

# sgcl::net::http::proxy::proxy

```cpp
proxy() noexcept;                              // (1)
explicit proxy(const string& url) noexcept;    // (2)
```

1. No proxy: every request direct. `web.proxy = net::http::proxy()` turns off what the environment set.
2. Every request through the proxy at `url`, `http://` and `https://` alike: both members set to it, `no_proxy`
   empty.

The URL is not read here: a request that uses it reads it, and fails with `net::errc::invalid_url` or
`net::errc::unsupported_scheme` when it is not a proxy's.

## Parameters

| Parameter | Description |
|---|---|
| `url` | the proxy's URL: `http://`, `https://`, `socks5://`, `socks5h://`, with `user:password@` for its credentials |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::proxy none;
    println("'{}' '{}'", none.http, none.https);

    net::http::proxy one("socks5h://127.0.0.1:1080");
    println("{} {}", one.http, one.https);

    auto bad = net::http::proxy("gopher://x").for_url(net::url("http://example.com/"));
    println("{}", bad.error().code() == net::errc::unsupported_scheme);
}
```

Output:

```text
'' ''
socks5h://127.0.0.1:1080 socks5h://127.0.0.1:1080
true
```

## See also

- [from_environment](from_environment.md): the environment's
- [proxy](README.md)

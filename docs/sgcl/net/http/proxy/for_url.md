[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [proxy](README.md)

# sgcl::net::http::proxy::for_url

```cpp
expected<optional<net::url>, io::error> for_url(const net::url& target) const noexcept;
```

The proxy a request to `target` goes through, as the client chooses it: `https` for an `https://` URL, `http` for an
`http://` one, none for another scheme, for an empty member, and for a host `no_proxy` names (with the URL's port, or
its scheme's). Go's `Transport.Proxy` asked for one request.

The proxy's URL is given as the client takes it: its scheme written (`http://` when the member has none), its port
(the scheme's when none is written), its credentials as written.

## Parameters

| Parameter | Description |
|---|---|
| `target` | the URL of a request |

## Return value

The proxy's URL; `nullopt` for a request that goes direct; or the [io::error](../../../io/error/README.md) of a member
that is not a proxy's URL, operation `proxy`: `net::errc::invalid_url` (not a URL, no host, a port of 0),
`net::errc::unsupported_scheme` (a scheme other than `http`, `https`, `socks5` and `socks5h`).

## Complexity

Linear in the size of `no_proxy` and of the proxy's URL.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::proxy p;
    p.http = "proxy.example:3128";
    p.https = "socks5h://user:pw@10.0.0.1";
    p.no_proxy = "internal.example, 192.168.0.0/16, example.org:8080";

    for (const char* u : {"http://example.com/", "https://example.com/", "http://db.internal.example/",
                          "http://192.168.1.10/", "http://example.org:8080/", "http://example.org/"}) {
        optional<net::url> via = p.for_url(net::url(u)).value();
        println("{} -> {}", u, via ? via->to_string() : string("direct"));
    }
}
```

Output:

```text
http://example.com/ -> http://proxy.example:3128/
https://example.com/ -> socks5h://user:pw@10.0.0.1:1080
http://db.internal.example/ -> direct
http://192.168.1.10/ -> direct
http://example.org:8080/ -> direct
http://example.org/ -> http://proxy.example:3128/
```

## See also

- [from_environment](from_environment.md): the environment's
- [proxy](README.md)

[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::proxy

```cpp
#include "sgcl/net/http/proxy.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    struct proxy;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::proxy` is which proxy a request of the [client](../client/README.md) goes through: the URL of the
proxy of `http://` requests, the URL of the proxy of `https://` ones, and the hosts reached directly, NO_PROXY's
list. A client takes the environment's when it is made, [from_environment](from_environment.md), as curl and Go's
`ProxyFromEnvironment` do; a proxy of the program's replaces it (`web.proxy = net::http::proxy(url)`), and a
default-constructed one says none. Go's `Transport.Proxy` is a function of the request; here it is a value of three
strings, read by each request, which [for_url](for_url.md) answers for one URL.

A proxy's URL is `http://` (an HTTP proxy), `https://` (one reached over TLS), `socks5://` (SOCKS5, the target's name
resolved by the program) or `socks5h://` (resolved by the proxy), the scheme `http://` when none is written
(`proxy.example:3128`), the port the scheme's (80, 443, 1080) when none is. Credentials in it, `user:password@`,
percent-decoded, are the proxy's: Basic `Proxy-Authorization` (RFC 7617) for an HTTP proxy, RFC 1929's username and
password for SOCKS5.

## Rules

- **How a request goes**: an `http://` request through an HTTP proxy, over TLS to an `https://` one, is sent to the
  proxy with its target in absolute-form (RFC 9112 §3.2.2: `GET http://example.com/a HTTP/1.1`), the proxy's
  credentials in `Proxy-Authorization` unless the request has that field of its own; the connection to the proxy
  serves every `http://` origin, and a 407 is the response, as in Go. An `https://` request goes through a tunnel:
  `CONNECT example.com:443` (RFC 9110 §9.3.6) with the credentials, then TLS to the origin over it, HTTP/2 when the
  origin chooses it by ALPN. Through SOCKS5 every request is a tunnel of [socks5](../../socks5/README.md), TLS over it
  for `https://`. The client's `tls` settings verify an `https://` proxy as an origin, by its name, with ALPN
  `http/1.1` alone.
- **The pool** keeps the routes apart: a tunnel's connections under the proxy (its credentials included) and the
  origin, the forwarded ones under the proxy alone, a direct one under the origin.
- **Errors** of the way through name the request and the proxy, never its credentials: `GET https://example.com/
  (through proxy http://127.0.0.1:3128, which answered 407 Proxy Authentication Required): proxy authentication
  required`. A CONNECT answered 407 is `net::errc::proxy_auth_required`, with another status but 2xx
  `net::errc::proxy_refused`, with what is not a response (or bytes behind a 2xx's head, which no proxy sends before
  the client speaks) `net::errc::malformed_proxy_response`; SOCKS5's are [socks5::connect](../../socks5/connect.md)'s.
  A URL that is not a proxy's fails the request with `net::errc::invalid_url` or `net::errc::unsupported_scheme`,
  the URL shown without its credentials.
- **NO_PROXY**: entries apart by commas or white space, each matched alone. A name matches itself and every name under
  it (`example.com`, `.example.com` and `*.example.com` alike, as curl reads them; Go's `.example.com` leaves out
  `example.com` itself), without regard to case or a trailing dot; an IPv4 or IPv6 address (brackets or not) and an
  IP network in CIDR (`10.0.0.0/8`) match a URL written with an address in them; any entry may carry a port
  (`example.com:8080`, `[::1]:8080`), and then matches that port alone; `*` matches every host. Nothing is resolved,
  and nothing is direct by itself: `localhost` goes through the proxy unless the list names it (curl's way; Go leaves
  localhost and the loopback out).
- A `socks5://` proxy is sent the first IPv4 address of the name, the first address when it has none: a SOCKS5
  request carries one.

## Member objects

| Member | Description |
|---|---|
| `string http` | the proxy's URL for `http://` requests; empty, the default, is direct |
| `string https` | the proxy's URL for `https://` requests; empty, the default, is direct |
| `string no_proxy` | the hosts reached directly, NO_PROXY's list (`"localhost,.internal,10.0.0.0/8"`, `"*"`); empty by default |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](proxy.md) | none, or one proxy for every request |
| [from_environment](from_environment.md) | the environment's: `http_proxy`, `HTTPS_PROXY`, `ALL_PROXY`, `NO_PROXY` (static) |
| [for_url](for_url.md) | the proxy a request to a URL goes through |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /hello", [](net::http::request, net::http::response_writer w) {
        w.write("hello through the proxy\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/hello";

    net::http::client web;
    web.proxy = net::http::proxy("http://127.0.0.1:3128");
    print("{}", web.get(url)->text().value());

    web.proxy.no_proxy = "127.0.0.1";  // direct
    print("{}", web.get(url)->text().value());
    srv.close();
}
```

Output:

```text
hello through the proxy
hello through the proxy
```

## See also

- [client](../client/README.md): its member `proxy`
- [socks5](../../socks5/README.md): the SOCKS5 connections under `socks5://` and `socks5h://`
- [errc](../../errc.md): `proxy_refused`, `proxy_auth_required`, `malformed_proxy_response`
- RFC 9110 §9.3.6 (CONNECT), RFC 9112 §3.2.2 (absolute-form), RFC 7617 (Basic); `tests/net/http/proxy.cpp` (a
  forward proxy written there, the environment, NO_PROXY by table, a proxy written in Go)

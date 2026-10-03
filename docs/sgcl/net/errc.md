[sgcl](../README.md) › [net](README.md)

# sgcl::net::errc

```cpp
#include "sgcl/net/error.h"   // or "sgcl/net.h"

namespace sgcl::net {
    enum class errc {
        invalid_address = 1,
        host_not_found,
        no_suitable_address,
        invalid_url,
        unsupported_scheme,
        malformed_response,
        header_too_large,
        body_too_large,
        too_many_redirects,
        server_closed,
        invalid_cookie,
        http_status
    };
}

template<>
struct std::is_error_code_enum<sgcl::net::errc> : std::true_type {};
```

The failures of net that neither `errno` nor [io](../io/errc.md) names: the sockets' and the resolver's, and
HTTP's ([net::http](http/README.md)). They form the category of the module, `"net"` ([category](category.md)),
beside the system one, io's and the resolver's ([lookup_category](lookup_category.md)). Everything net reports is an
[io::error](../io/error.md) in an `expected<T, io::error>`, one error for all where Go has `net.ErrClosed`,
`os.ErrDeadlineExceeded` and `*net.DNSError`: its code is an `errno` value in the system category
(`ECONNREFUSED`, `ETIMEDOUT` for a deadline, `ECANCELED` for a stop, `EPIPE`), io's `errc::closed` for an
operation on a connection the program closed, one of these, or an `EAI_*` code of the resolver. The operation names
what failed as Go does (`dial tcp`, `listen tcp`, `read`, `accept`, `lookup`), the path what it was on, so that
`message()` reads `lookup db.internal: no such host`.

`std::is_error_code_enum` is specialized, so an `errc` converts to an `error_code`
([make_error_code](make_error_code.md)) and `e.code() == net::errc::host_not_found` compares directly. The
predicates of `io::error` answer across the categories: `is_timeout()` for a deadline, `is_closed()` for a close by
the program, `is_not_found()` for a unix socket's path that is not there.

| Value | Description |
|---|---|
| `invalid_address` | "invalid address": a text that [ip_address](ip_address.md), [ip_network](ip_network.md) or [endpoint](endpoint.md)`::parse` does not read as one; a `"host:port"` that cannot be taken apart (no port, a port past 65535 or not a number, an IPv6 host without brackets); a unix path too long for `sun_path` |
| `host_not_found` | "no such host": the resolver knows no address of the name (`EAI_NONAME`); an empty host to look up |
| `no_suitable_address` | "no suitable address found": a dial with nothing to dial, every address the name resolved to of no use |
| `invalid_url` | "invalid URL": a text [url](url.md)`::parse` does not read as one, or a value a setter of `url` refuses; a text past 512 MiB, or one whose URL would pass it ([the limit](url.md#rules)); a query text past 512 MiB, or pairs [query_params](query_params.md) would write past it ([its limit](query_params.md#rules)); a request's URL that is not one, reported by the send |
| `unsupported_scheme` | "unsupported protocol scheme": a URL whose scheme the client cannot speak, neither `http` nor `https` |
| `malformed_response` | "malformed HTTP response": a response that breaks RFC 9112, its head or its framing |
| `header_too_large` | "header too large": a head past its limit |
| `body_too_large` | "body too large": a read of a body past its limit |
| `too_many_redirects` | "stopped after too many redirects": more than the client's `max_redirects`, 10 by default |
| `server_closed` | "server closed": `serve` after `shutdown()` or `close()`, Go's `ErrServerClosed` |
| `invalid_cookie` | "invalid cookie": a `Set-Cookie` value that [cookie](http/cookie.md)`::parse` finds no cookie in |
| `http_status` | "the response's status is not 2xx": [http::download](http/download.md) of a URL that answered 404 or 500, nothing written |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    auto c = net::tcp::connect("127.0.0.1");  // no port
    println("{} {}", c.error().code() == net::errc::invalid_address, c.error().message());

    auto found = net::dns::lookup("");
    println("{}", found.error().code() == net::errc::host_not_found);

    error_code code = net::errc::too_many_redirects;
    println("{}: {}", code.category().name(), code.message());
}
```

Output:

```text
true dial tcp 127.0.0.1: invalid address
true
net: stopped after too many redirects
```

## See also

- [io::error](../io/error.md): the code, the operation, the path, the predicates
- [category](category.md), [lookup_category](lookup_category.md), [make_error_code](make_error_code.md)
- `tests/net/socket.cpp` (`ErrorsOfConnectAndListen`), `tests/net/dial.cpp` (`Names`)

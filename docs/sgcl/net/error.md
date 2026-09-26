# sgcl::net::errc

```cpp
#include "sgcl/net/error.h"   // or "sgcl/net/net.h"

namespace sgcl::net {
    enum class errc { invalid_address = 1, host_not_found, no_suitable_address,   // the sockets and the resolver
                      invalid_url, unsupported_scheme, malformed_response,       // HTTP
                      header_too_large, body_too_large, too_many_redirects, server_closed,
                      invalid_cookie };
    const std::error_category& category() noexcept;          // "net"
    const std::error_category& lookup_category() noexcept;   // "lookup": the EAI_* codes of getaddrinfo, gai_strerror's text
    error_code make_error_code(errc e) noexcept;
}
```

The failures of the module that neither `errno` nor [io](../io/error.md) names: the sockets' and the resolver's, and HTTP's ([http](http/README.md)). Everything net reports is an [`io::error`](../io/error.md) in an `expected<T, io::error>`: its code is an `errno` value in the system category (`ECONNREFUSED`, `ETIMEDOUT` for a deadline, `ECANCELED` for a stop, `EPIPE`), io's `errc::closed` for an operation on a connection the program closed, one of these, or an `EAI_*` code in `lookup_category()`. The operation names what failed as Go does (`"dial tcp"`, `"listen tcp"`, `"read"`, `"accept"`, `"lookup"`), the path what it was on (the address given, or `"tcp 1.2.3.4:5->6.7.8.9:80"`), so that `message()` reads `dial tcp 127.0.0.1:1: Connection refused`.

## Rules

- `EAI_*` values are not `errno` values (on macOS `EAI_AGAIN` is 2, which is `ENOENT`): a code from the resolver is in `lookup_category()`, never in the system one. `EAI_NONAME` (and `EAI_NODATA` where it exists) is `errc::host_not_found`, `EAI_SYSTEM` the `errno` it stands for.
- `std::is_error_code_enum<net::errc>` is specialized: `e.code() == net::errc::host_not_found` compares directly.
- The predicates of `io::error` answer across the categories: `is_timeout()` for a deadline, `is_closed()` for a close by the program, `is_not_found()` for a unix socket's path that is not there.

## Members

### errc

```cpp
enum class errc {
    invalid_address = 1,   // "invalid address": a text ip_address, ip_network or endpoint::parse does not read as one; "host:port" that cannot be taken apart: no port, a port past 65535 or not a number, an IPv6 host without brackets; a unix path too long for sun_path
    host_not_found,        // "no such host": the resolver knows no address of the name (EAI_NONAME); an empty host to look up
    no_suitable_address,   // "no suitable address found": a dial with nothing to dial: every address the name resolved to was of no use
    invalid_url,           // "invalid URL": a text url::parse does not read as one; a request's URL that is not one, reported by the send
    unsupported_scheme,    // "unsupported protocol scheme": a URL the client cannot speak (https:// until TLS)
    malformed_response,    // "malformed HTTP response": a response that breaks RFC 9112, its head or its framing
    header_too_large,      // "header too large": a head past its limit
    body_too_large,        // "body too large": a read of a body past its limit
    too_many_redirects,    // "stopped after too many redirects": more than the client's max_redirects (10 by default)
    server_closed,         // "server closed": serve after shutdown() or close(), Go's ErrServerClosed
    invalid_cookie         // "invalid cookie": a Set-Cookie value that cookie::parse finds no cookie in
};
```

## Example

```cpp
auto c = net::tcp::connect("db.internal:5432", 3s);
if (!c) {
    auto& e = c.error();
    if (e.code() == net::errc::host_not_found) { /* a name nobody knows */ }
    else if (e.is_timeout()) { /* nobody answered in 3 s */ }
    else if (e.code() == std::errc::connection_refused) { /* nothing listens there */ }
    std::cerr << e.message() << '\n';   // "lookup db.internal: no such host", "dial tcp 10.0.0.7:5432: Connection refused"
}
```

## See also

- [io error](../io/error.md): `io::error`, the predicates
- `tests/net/socket.cpp` (`ErrorsOfConnectAndListen`), `tests/net/dial.cpp` (`Names`)

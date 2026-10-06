[sgcl](../../../README.md) › [net](../../README.md) › [pop3](../README.md) › [server](README.md)

# sgcl::net::pop3::server::serve_tls, async_serve_tls

```cpp
expected<void, io::error> serve_tls(const string& address, const net::tls::config& c) const;
async::task<expected<void, io::error>> async_serve_tls(const string& address, const net::tls::config& c) const noexcept;
```

Listens with TLS from the first byte (pop3s, port 995, RFC 8314) with the config, and serves as [serve](serve.md) does.

`serve_tls` blocks the calling thread; a task awaits `async_serve_tls`.

## Parameters

| Parameter | Description |
|---|---|
| `address` | where to listen |
| `c` | the [tls::config](../../tls/config.md): the identity |

## Return value

`net::errc::server_closed` after a shutdown or a close; the listen's error.

## Complexity

Each connection a task, a TLS handshake each.

## Exceptions

- `serve_tls`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_serve_tls`: none.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/imap.h"
#include "sgcl/net/pop3.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::imap::memory_backend mail;
    mail.add_user("alice", "secret");
    mail.append("alice", "INBOX", "From: bob@example.com\r\nSubject: Lunch\r\n\r\nNoon?\r\n");
    net::pop3::server srv;
    srv.backend = mail;
    net::tls::config tls;
    tls.identities = {net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                                         crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    auto serving = async::spawn(srv.async_serve_tls("127.0.0.1:0", tls));
    srv.close();
    println("{}", serving.wait().error().code() == net::errc::server_closed);
}
```

Output:

```text
true
```

## See also

- [serve](serve.md)
- [server](README.md)

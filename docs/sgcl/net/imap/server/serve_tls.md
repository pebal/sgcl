[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [server](README.md)

# sgcl::net::imap::server::serve_tls, async_serve_tls

```cpp
expected<void, io::error> serve_tls(const string& address, const net::tls::config& c) const;         // (1)
async::task<expected<void, io::error>> async_serve_tls(const string& address,                        // (2)
                                                       const net::tls::config& c) const noexcept;
```

Listens over TLS from the first byte (port 993, RFC 8314) with the config, and serves as [serve](serve.md) does.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `address` | the address: `":993"` |
| `c` | the [TLS config](../../tls/config.md): the server's identity |

## Return value

`net::errc::server_closed` after shutdown or close; or the error of the listen (a config without an identity among
them).

## Complexity

That of the connections served.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/imap.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    net::imap::memory_backend mail;
    mail.add_user("alice", "secret");
    net::imap::server srv;
    srv.backend = mail;
    net::tls::config tls;
    tls.identities = {net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                                         crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    net::listener listener = net::tls::listen("127.0.0.1:0", tls);
    auto serving = async::spawn(srv.async_serve(listener));

    net::imap::client::options o;
    o.tls.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    string url = "imaps://alice:secret@localhost:" + to_string(listener.local_endpoint().port());
    net::imap::client session = net::imap::client::connect(url, o);
    println("{}", session.is_tls());
    srv.close();
}
```

Output:

```text
true
```

## See also

- [serve](serve.md)
- [tls::config](../../tls/config.md)
- [sgcl::net::imap::server](README.md)

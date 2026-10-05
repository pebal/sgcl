[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [server](README.md)

# sgcl::net::smtp::server::serve_tls, async_serve_tls

```cpp
expected<void, io::error> serve_tls(const string& address, const tls::config& c) const;
async::task<expected<void, io::error>> async_serve_tls(const string& address,
                                                       const tls::config& c) const noexcept;
```

Listens over TLS from the first byte, port 465's submission (RFC 8314), and serves as [serve](serve.md) does; the
sessions' envelopes say `tls`. STARTTLS is not offered on them.

## Parameters

| Parameter | Description |
|---|---|
| `address` | where to listen |
| `c` | the server's [tls::config](../../tls/config.md): its identity |

## Return value

As [serve](serve.md)'s.

## Complexity

As long as the server runs.

## Exceptions

- `serve_tls`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_serve_tls`: none.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net/tls.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::tls::config cert;
    cert.identities = {net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                                          crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    net::smtp::server srv;
    srv.handle([](net::smtp::message m) { println("over TLS: {}", m.envelope().tls); });
    net::listener l = net::tls::listen("127.0.0.1:0", cert);
    auto serving = async::spawn(srv.async_serve(l));

    net::smtp::options o;
    string ca = io::read_text("tests/net/tls_testdata/ca.pem");
    o.tls.roots = crypto::x509::certificate_pool::from_pem(ca);
    o.tls.server_name = "localhost";
    string url = string::concat("smtps://127.0.0.1:", to_string(l.local_endpoint().port()));
    net::smtp::send(url, encoding::email("a@example.com", "b@example.org", "Hi", "Hello."), o);
    srv.close();
    serving.wait();
}
```

Output:

```text
over TLS: true
```

## See also

- [serve](serve.md)
- [server](README.md)

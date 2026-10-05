[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [client](README.md)

# sgcl::net::smtp::client::connect, async_connect

```cpp
static expected<client, io::error> connect(const string& url, const options& o = {});
static async::task<expected<client, io::error>> async_connect(string url, options o = {}) noexcept;
```

A session opened with the server of `url`: the connection (TLS from the first byte for `smtps://`), the greeting
(220), EHLO and its extensions (HELO when the server refuses EHLO with a 5xx), STARTTLS when offered and
`o.starttls` is on (EHLO again over TLS), AUTH when there are credentials.

## Parameters

| Parameter | Description |
|---|---|
| `url` | `smtp://[user:password@]host[:port]` or `smtps://...` |
| `o` | the credentials, TLS, the waits, the stop |

## Return value

The session; or the [io::error](../../../io/error/README.md) of the connection, of TLS, `net::errc::smtp_reply` (a refused greeting or EHLO), `smtp_auth_failed`, `smtp_tls_required`, `smtp_unsupported`, `malformed_smtp_reply`, `invalid_url`, `unsupported_scheme`, `ETIMEDOUT`, `ECANCELED`.

## Complexity

Four round trips (six with STARTTLS, more with AUTH LOGIN), and the TLS handshake.

## Exceptions

- `connect`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_connect`: none.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::smtp::server srv;
    srv.hostname = "mx.example";
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    string url = string::concat("smtp://", l.local_endpoint().to_string());

    net::smtp::client c = net::smtp::client::connect(url);
    println("{} | {}", c.greeting(), c.has_extension("PIPELINING"));
    c.quit();
    println("{}", net::smtp::client::connect("smtp://127.0.0.1:1").error().message());
    srv.close();
    serving.wait();
}
```

Output:

```text
mx.example ESMTP ready | true
dial tcp 127.0.0.1:1 (its only address: 127.0.0.1:1: Connection refused): Connection refused
```

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
    srv.starttls = cert;
    srv.auth = [](const string& user, const string& password) {
        return user == "alice" && password == "s3cret";
    };
    srv.handle([](net::smtp::message m) { println("from user {}", m.envelope().user); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));

    net::smtp::options o;
    string ca = io::read_text("tests/net/tls_testdata/ca.pem");
    o.tls.roots = crypto::x509::certificate_pool::from_pem(ca);
    o.tls.server_name = "localhost";
    o.require_tls = true;
    string port = to_string(l.local_endpoint().port());
    string url = string::concat("smtp://alice:s3cret@127.0.0.1:", port);
    net::smtp::client c = net::smtp::client::connect(url, o);
    println("{}", c.is_tls());
    c.send(encoding::email("alice@example.com", "bob@example.org", "Hi", "Hello."));
    c.quit();
    srv.close();
    serving.wait();
}
```

Output:

```text
true
from user alice
```

## See also

- [send](send.md)
- [options](../options.md)
- [client](README.md)

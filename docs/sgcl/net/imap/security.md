[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::security

```cpp
#include "sgcl/net/imap/client.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    enum class security : uint8_t { automatic, tls, starttls, none };
}
```

How a [client](client/README.md)'s connection is protected, [client::options](client-options.md)'s `security`. RFC
8314 asks for TLS from the first byte; RFC 9051 still has STARTTLS on port 143, and a client that would go on in the
clear when a server does not offer it gives the credentials to whoever sits in between (a downgrade): `automatic`
therefore requires STARTTLS, and the connect fails with `errc::starttls_unavailable` rather than send a password in
the clear.

| Value | Description |
|---|---|
| `automatic` | TLS from the first byte for `imaps://` and port 993; STARTTLS, required, for the rest (the default) |
| `tls` | TLS from the first byte, whatever the port (993 when the address gives none) |
| `starttls` | STARTTLS, required, whatever the port |
| `none` | no TLS: the loopback, a test, a network of one's own |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/imap.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::imap::memory_backend mail;
    mail.add_user("alice", "secret");
    mail.append("alice", "INBOX", "From: Bob <bob@example.com>\r\nSubject: Lunch\r\n\r\nNoon?\r\n");
    net::imap::server srv;
    srv.backend = mail;
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));

    net::imap::client::options o;
    o.user = "alice";
    o.password = "secret";
    o.security = net::imap::security::none;   // the loopback, no TLS
    string address = "127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::imap::client session = net::imap::client::connect(address, o);
    println("{}", session.is_tls());
    srv.close();
}
```

Output:

```text
false
```

## See also

- [client::options](client-options.md)
- [connect](client/connect.md)
- [tls::config](../tls/config.md): the TLS settings, options' `tls`
- [sgcl::net::imap](README.md)

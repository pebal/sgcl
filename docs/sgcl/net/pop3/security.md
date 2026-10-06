[sgcl](../../README.md) › [net](../README.md) › [pop3](README.md)

# sgcl::net::pop3::security

```cpp
#include "sgcl/net/pop3/types.h"   // or "sgcl/net/pop3.h"

namespace sgcl::net::pop3 {
    enum class security : uint8_t { automatic, tls, starttls, none };
}
```

How a [client](client/README.md) protects its connection, the [options](client-options.md)' `security`.

| Value | Description |
|---|---|
| `automatic` | TLS from the first byte for `pop3s://` and port 995; STLS for the rest, a server without it refused |
| `tls` | TLS from the first byte (RFC 8314) |
| `starttls` | STLS (RFC 2595) before the credentials, a server without it refused |
| `none` | no TLS: a test's loopback, a tunnel that is protected already |

## Example

```cpp
#include "sgcl/async.h"
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
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    net::pop3::client::options o;
    o.user = "alice";
    o.password = "secret";
    auto c = net::pop3::client::connect(l.local_endpoint().to_string(), o);
    println("{}", c.error().code() == net::pop3::errc::starttls_unavailable);
    srv.close();
    serving.wait();
}
```

Output:

```text
true
```

## See also

- [client::options](client-options.md)
- [pop3](README.md)

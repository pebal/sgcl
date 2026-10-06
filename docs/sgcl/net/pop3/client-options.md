[sgcl](../../README.md) › [net](../README.md) › [pop3](README.md) › [client](client/README.md) › options

# sgcl::net::pop3::client::options

```cpp
#include "sgcl/net/pop3/client.h"   // or "sgcl/net/pop3.h"

namespace sgcl::net::pop3 {
    class client {
    public:
        struct options {
            string user;
            string password;
            pop3::mechanism mechanism = pop3::mechanism::automatic;
            pop3::security security = pop3::security::automatic;
            net::tls::config tls;
            duration timeout = std::chrono::seconds(30);
            async::stop_token stop;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::pop3::client::options` is how a [client](client/README.md) talks to its server, the argument of
[connect](client/connect.md): the credentials, how they are sent, how the connection is protected, the waits.

## Member objects

| Member | Description |
|---|---|
| `user`, `password` | the login; empty: the URL's, or no login |
| `mechanism` | the [mechanism](mechanism.md); `automatic` by default: SASL PLAIN when offered, else USER and PASS |
| `security` | the [security](security.md); `automatic` by default: TLS for `pop3s://` and port 995, STLS for the rest |
| `tls` | the [tls::config](../tls/config.md) of `pop3s://` and STLS: the roots, a client certificate; the server's name the address's host when none is set |
| `timeout` | the dial, TLS and the login together, then each reply's wait; 30 s by default; zero: none |
| `stop` | ends the dial with `ECANCELED` |

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
    o.mechanism = net::pop3::mechanism::user;
    o.security = net::pop3::security::none;  // the loopback, no TLS
    auto c = net::pop3::client::connect(l.local_endpoint().to_string(), o);
    println("{}", c.has_value());
    c->quit();
    o.security = net::pop3::security::starttls;
    auto refused = net::pop3::client::connect(l.local_endpoint().to_string(), o);
    println("{}", refused.error().code() == net::pop3::errc::starttls_unavailable);
    srv.close();
    serving.wait();
}
```

Output:

```text
true
true
```

## See also

- [connect](client/connect.md)
- [client](client/README.md)

[sgcl](../../README.md) › [net](../README.md) › [pop3](README.md)

# sgcl::net::pop3::mechanism

```cpp
#include "sgcl/net/pop3/types.h"   // or "sgcl/net/pop3.h"

namespace sgcl::net::pop3 {
    enum class mechanism : uint8_t { automatic, plain, user, apop };
}
```

How a [client](client/README.md) logs in, the [options](client-options.md)' `mechanism`.

| Value | Description |
|---|---|
| `automatic` | SASL PLAIN when the server offers it, else USER and PASS |
| `plain` | AUTH PLAIN (RFC 5034), the server must offer it |
| `user` | USER and PASS (RFC 1939 §7) |
| `apop` | APOP: the MD5 of the greeting's timestamp and the password, the password never sent; the server must offer it with a timestamp |

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
    net::pop3::server srv;
    srv.backend = mail;
    srv.apop_secret = [](const string& user) -> optional<string> {
        return user == "alice" ? optional<string>("secret") : nullopt;
    };
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    net::pop3::client::options o;
    o.user = "alice";
    o.password = "secret";
    o.security = net::pop3::security::none;  // the loopback, no TLS
    o.mechanism = net::pop3::mechanism::apop;
    auto c = net::pop3::client::connect(l.local_endpoint().to_string(), o);
    println("{}", c.has_value());
    c->quit();
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

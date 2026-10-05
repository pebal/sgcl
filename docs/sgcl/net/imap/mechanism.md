[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::mechanism

```cpp
#include "sgcl/net/imap/client.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    enum class mechanism : uint8_t { automatic, plain, login, xoauth2, oauthbearer, login_command };
}
```

The way a [client](client/README.md) logs in, [client::options](client-options.md)'s `mechanism`. `automatic` chooses
by what the options hold and the server offers: for a token OAUTHBEARER (RFC 7628) when offered, else XOAUTH2; for a
password AUTHENTICATE PLAIN, else the LOGIN command, else AUTHENTICATE LOGIN. With SASL-IR (RFC 4959) the first answer
goes with the command, saving a round trip.

| Value | Description |
|---|---|
| `automatic` | chosen as above (the default) |
| `plain` | AUTHENTICATE PLAIN (RFC 4616): the user and the password in one answer |
| `login` | AUTHENTICATE LOGIN: the user and the password each asked for |
| `xoauth2` | AUTHENTICATE XOAUTH2: the user and an OAuth 2.0 access token, Google's and Microsoft's form |
| `oauthbearer` | AUTHENTICATE OAUTHBEARER (RFC 7628): the user and an OAuth 2.0 bearer token |
| `login_command` | the LOGIN command of RFC 9051, no SASL |

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
    o.mechanism = net::imap::mechanism::plain;
    string address = "127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::imap::client session = net::imap::client::connect(address, o);
    println("{}", session.has("AUTH=PLAIN"));
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
- [sgcl::net::imap](README.md)

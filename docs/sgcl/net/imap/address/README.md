[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md)

# sgcl::net::imap::address

```cpp
#include "sgcl/net/imap/types.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    struct address {
        string name;
        string mailbox;
        string host;

        string email() const noexcept;
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

An address of an [envelope](../envelope.md) (RFC 9051 §7.5.2): the display name, the mailbox (the local part, before
"@") and the host, as the server took them apart from the message's header. The client decodes the encoded words of
the name (RFC 2047, `=?UTF-8?Q?...?=`) into UTF-8; the mailbox and the host are as written. The members of a group
(`team: a@x, b@y;`) come as addresses of their own, the group's name dropped.

## Member objects

| Member | Description |
|---|---|
| `name` | the display name, decoded; empty when the address has none |
| `mailbox` | the local part: `bob` of `bob@example.com` |
| `host` | the domain: `example.com`; empty for an address without one |

## Member functions

| Function | Description |
|---|---|
| [email](email.md) | `mailbox@host` |

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
    mail.append("alice", "INBOX", "To: =?UTF-8?Q?Zo=C3=AB?= <zoe@example.com>, ops@example.org\r\n"
                              "Subject: hi\r\n\r\nx\r\n");
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
    session.select("INBOX");
    net::imap::fetch_options what;
    what.envelope = true;
    auto messages = session.fetch(1, what);
    for (const net::imap::address& a : (*messages)[0].envelope->to) {
        println("{} <{}>", a.name, a.email());
    }
    srv.close();
}
```

Output:

```text
Zoë <zoe@example.com>
 <ops@example.org>
```

## See also

- [envelope](../envelope.md)
- [sgcl::net::imap](../README.md)

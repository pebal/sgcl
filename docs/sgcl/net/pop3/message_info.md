[sgcl](../../README.md) › [net](../README.md) › [pop3](README.md) › message_info

# sgcl::net::pop3::message_info

```cpp
#include "sgcl/net/pop3/types.h"   // or "sgcl/net/pop3.h"

namespace sgcl::net::pop3 {
    struct message_info {
        uint32_t number = 0;
        uint64_t size = 0;
        string uid;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::pop3::message_info` is a message of the maildrop as [list](client/list.md) gives it: LIST's number and size joined with UIDL's unique id. Compared member by member (`==`).

## Member objects

| Member | Description |
|---|---|
| `number` | its number in the session, from 1 |
| `size` | its size in bytes, as the server counts it (CRLF line breaks) |
| `uid` | its unique id, the same in every session while the server keeps the message; empty when the server has no UIDL |

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
    o.security = net::pop3::security::none;  // the loopback, no TLS
    net::pop3::client c = net::pop3::client::connect(l.local_endpoint().to_string(), o).value();
    net::pop3::message_info m = c.list(1).value();
    println("{} {} {}", m.number, m.size, m.uid.size() > 0);
    c.quit();
    srv.close();
    serving.wait();
}
```

Output:

```text
1 48 true
```

## See also

- [client](client/README.md)
- [pop3](README.md)

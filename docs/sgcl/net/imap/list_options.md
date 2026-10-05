[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::list_options

```cpp
#include "sgcl/net/imap/client.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    struct list_options {
        string reference;
        bool subscribed = false;
        bool special_use = false;
        bool status = false;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

What [list](client/list.md) asks for (RFC 9051 §6.3.9, LIST-EXTENDED, SPECIAL-USE, LIST-STATUS). A server without
LIST-EXTENDED is asked by LSUB for the subscribed mailboxes, and its special ones are picked by their attributes.

## Member objects

| Member | Description |
|---|---|
| `reference` | the reference name the pattern is taken under; empty by default |
| `subscribed` | the subscribed mailboxes only |
| `special_use` | the special-use mailboxes only |
| `status` | each mailbox's [status](status.md) with it, in `list_entry::status` |

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
    session.create("Archive");
    session.subscribe("Archive");
    net::imap::list_options which;
    which.subscribed = true;
    auto boxes = session.list("*", which);
    println("{}", (*boxes)[0].name);
    srv.close();
}
```

Output:

```text
Archive
```

## See also

- [list](client/list.md), [list_entry](list_entry/README.md)
- [sgcl::net::imap](README.md)

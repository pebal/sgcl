[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md)

# sgcl::net::imap::list_entry

```cpp
#include "sgcl/net/imap/types.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    struct list_entry {
        string name;
        char delimiter = '/';
        vector<string> attributes;
        optional<imap::status> status;

        bool has_attribute(const string& a) const noexcept;
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

A mailbox as LIST gave it (RFC 9051 §7.3.1): its name in UTF-8, the hierarchy delimiter, its attributes and, when
[list_options](../list_options.md) asked for it, its [status](../status.md). The attributes are LIST's:
`\HasChildren`, `\HasNoChildren`, `\Noselect` and `\NonExistent` for a parent that is no mailbox, `\Subscribed`,
and the special uses (`\Sent`, `\Trash` ...). A [backend](../backend/README.md) gives its mailboxes as list entries
too: their special use and `\Subscribed`, `\NonExistent` for a name subscribed without a mailbox.

## Member objects

| Member | Description |
|---|---|
| `name` | the name, UTF-8: `INBOX`, `Archive/2026` |
| `delimiter` | the hierarchy delimiter; `0` for a server without hierarchy |
| `attributes` | the attributes, as written |
| `status` | the status, with `list_options::status` |

## Member functions

| Function | Description |
|---|---|
| [has_attribute](has_attribute.md) | whether the entry has an attribute |

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
    session.create("Archive/2026");
    session.create("Sent", net::imap::special_use::sent);
    auto boxes = session.list();
    for (const net::imap::list_entry& e : *boxes) {
        println("{} {}", e.name, e.attributes);
    }
    srv.close();
}
```

Output:

```text
INBOX ["\\HasNoChildren"]
Archive ["\\NonExistent", "\\HasChildren"]
Archive/2026 ["\\HasNoChildren"]
Sent ["\\HasNoChildren", "\\Sent"]
```

## See also

- [list](../client/list.md), [list_options](../list_options.md)
- [sgcl::net::imap](../README.md)

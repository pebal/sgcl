[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::namespaces

```cpp
#include "sgcl/net/imap/types.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    struct namespaces {
        vector<namespace_entry> personal;
        vector<namespace_entry> other_users;
        vector<namespace_entry> shared;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

The namespaces of a server (NAMESPACE, RFC 2342): the user's own mailboxes, other users', shared ones. The module's
server has one personal namespace, `""` with `/`.

## Member objects

| Member | Description |
|---|---|
| `personal` | the user's own |
| `other_users` | other users' mailboxes |
| `shared` | shared mailboxes |

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
    net::imap::namespaces n = session.namespaces();
    println("{} {} {}", n.personal.size(), n.other_users.size(), n.shared.size());
    srv.close();
}
```

Output:

```text
1 0 0
```

## See also

- [namespaces](client/namespaces.md)
- [namespace_entry](namespace_entry.md)
- [sgcl::net::imap](README.md)

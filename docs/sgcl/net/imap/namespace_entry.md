[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::namespace_entry

```cpp
#include "sgcl/net/imap/types.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    struct namespace_entry {
        string prefix;
        char delimiter = '/';
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

One namespace of a server (RFC 2342): the prefix of its mailboxes' names and its hierarchy delimiter.

## Member objects

| Member | Description |
|---|---|
| `prefix` | the prefix: `""` for the personal namespace of most servers, `"#shared/"` |
| `delimiter` | the delimiter; 0 for none |

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
    println("[{}] {}", n.personal[0].prefix, n.personal[0].delimiter);
    srv.close();
}
```

Output:

```text
[] /
```

## See also

- [namespaces](namespaces.md)
- [sgcl::net::imap](README.md)

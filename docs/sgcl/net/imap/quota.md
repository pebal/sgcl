[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::quota

```cpp
#include "sgcl/net/imap/types.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    struct quota {
        string root;
        uint64_t storage_used = 0;
        uint64_t storage_limit = 0;
        uint64_t messages_used = 0;
        uint64_t messages_limit = 0;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

A quota root's resources (RFC 9208): what is used and the limit, the storage in KiB. A limit of 0 is none. The
client's [quota](client/quota.md) gives those of a mailbox's roots; a backend's `quota` gives its user's (root `""`).

## Member objects

| Member | Description |
|---|---|
| `root` | the quota root |
| `storage_used`, `storage_limit` | KiB used, the limit |
| `messages_used`, `messages_limit` | messages, the limit |

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
    mail.set_quota("alice", 1024, 100);
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));

    net::imap::client::options o;
    o.user = "alice";
    o.password = "secret";
    o.security = net::imap::security::none;   // the loopback, no TLS
    string address = "127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::imap::client session = net::imap::client::connect(address, o);
    auto roots = session.quota("INBOX");
    for (const net::imap::quota& q : *roots) {
        println("{} of {} messages", q.messages_used, q.messages_limit);
    }
    srv.close();
}
```

Output:

```text
1 of 100 messages
```

## See also

- [quota](client/quota.md)
- [sgcl::net::imap](README.md)

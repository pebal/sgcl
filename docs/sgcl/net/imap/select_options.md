[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::select_options

```cpp
#include "sgcl/net/imap/client.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    struct select_options {
        bool read_only = false;
        uint32_t uid_validity = 0;
        uint64_t modseq = 0;
        sequence_set known_uids;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

What [select](client/select.md) asks for: EXAMINE, and QRESYNC's state from an earlier session (RFC 7162 §3.2.5). With
`uid_validity` and `modseq` of a [selected](selected.md) kept from before, the server answers with the UIDs expunged
since and the messages whose flags changed since, when the client has QRESYNC on (it enables it by itself where the
server offers it).

## Member objects

| Member | Description |
|---|---|
| `read_only` | EXAMINE: nothing changed, nothing marked seen |
| `uid_validity` | the mailbox's UIDVALIDITY as it was |
| `modseq` | the highest mod-sequence as it was |
| `known_uids` | the UIDs the client knows; empty: every one below UIDNEXT |

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
    net::imap::selected before = session.select("INBOX");
    session.add_flags(1, {net::imap::flag::seen});
    net::imap::select_options resync;
    resync.uid_validity = before.uid_validity;
    resync.modseq = before.highest_modseq;
    net::imap::selected after = session.select("INBOX", resync);
    println("{} changed: UID {}", after.changed.size(), after.changed[0].uid);
    srv.close();
}
```

Output:

```text
1 changed: UID 1
```

## See also

- [select](client/select.md), [selected](selected.md)
- [sgcl::net::imap](README.md)

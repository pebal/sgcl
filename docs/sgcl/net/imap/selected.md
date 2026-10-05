[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::selected

```cpp
#include "sgcl/net/imap/types.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    struct selected {
        string name;
        bool read_only = false;
        uint32_t exists = 0;
        uint32_t recent = 0;
        uint32_t uid_validity = 0;
        uint32_t uid_next = 0;
        uint32_t first_unseen = 0;
        uint64_t highest_modseq = 0;
        vector<string> flags;
        vector<string> permanent_flags;
        sequence_set vanished;
        vector<message> changed;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

What SELECT or EXAMINE said of the mailbox opened (RFC 9051 §6.3.2): the counts, the UIDs, the flags; with QRESYNC's
state from an earlier session ([select_options](select_options.md)) the UIDs expunged since and the messages changed
since (RFC 7162 §3.2.5), so that a client's copy is brought up to date in one round trip. [select](client/select.md)
returns it, and [mailbox](client/mailbox.md) gives it again with the counts the server's updates moved since.

## Member objects

| Member | Description |
|---|---|
| `name` | the mailbox |
| `read_only` | opened by EXAMINE, or READ-ONLY by the server |
| `exists` | the number of messages |
| `recent` | IMAP4rev1's recent messages |
| `uid_validity` | UIDVALIDITY |
| `uid_next` | the next UID |
| `first_unseen` | IMAP4rev1's UNSEEN: the number of the first message without `\Seen` |
| `highest_modseq` | HIGHESTMODSEQ; 0 for a mailbox without mod-sequences |
| `flags` | the flags the mailbox uses |
| `permanent_flags` | the flags a store keeps; `\*` when new keywords may be made |
| `vanished` | QRESYNC: the UIDs expunged since the state given |
| `changed` | QRESYNC: the messages changed since (UID, flags, mod-sequence) |

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
    net::imap::selected box = session.select("INBOX");
    println("{}: {} messages, next UID {}, read only {}", box.name, box.exists, box.uid_next, box.read_only);
    srv.close();
}
```

Output:

```text
INBOX: 1 messages, next UID 2, read only false
```

## See also

- [select](client/select.md), [select_options](select_options.md)
- [sgcl::net::imap](README.md)

[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::status

```cpp
#include "sgcl/net/imap/types.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    struct status {
        uint32_t messages = 0;
        uint32_t recent = 0;
        uint32_t uid_next = 0;
        uint32_t uid_validity = 0;
        uint32_t unseen = 0;
        uint32_t deleted = 0;
        uint64_t size = 0;
        uint64_t highest_modseq = 0;
    };
}
```

What STATUS tells of a mailbox without selecting it (RFC 9051 §6.3.11, CONDSTORE's HIGHESTMODSEQ, RFC 8438's SIZE):
the items the server has, the rest 0. [status](client/status.md) asks for every one; a
[list_entry](list_entry/README.md) of LIST-STATUS carries one.

## Member objects

| Member | Description |
|---|---|
| `messages` | the number of messages |
| `recent` | IMAP4rev1's recent messages |
| `uid_next` | the UID the next message will get |
| `uid_validity` | UIDVALIDITY: the UIDs are valid while it stays |
| `unseen` | the messages without `\Seen` |
| `deleted` | the messages with `\Deleted` |
| `size` | the octets of every message together |
| `highest_modseq` | the highest mod-sequence (CONDSTORE) |

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
    auto s = session.status("INBOX");
    println("{} messages, {} unseen, next UID {}", s->messages, s->unseen, s->uid_next);
    srv.close();
}
```

Output:

```text
1 messages, 1 unseen, next UID 2
```

## See also

- [status](client/status.md)
- [list_options](list_options.md)
- [sgcl::net::imap](README.md)

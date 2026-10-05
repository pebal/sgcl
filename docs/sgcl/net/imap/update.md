[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::update

```cpp
#include "sgcl/net/imap/types.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    struct update {
        enum class kind : uint8_t { exists, recent, expunge, vanished, fetch, flags, alert, bye };

        update::kind kind = kind::exists;
        uint32_t number = 0;
        uint32_t uid = 0;
        vector<string> flags;
        uint64_t modseq = 0;
        sequence_set uids;
        string text;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

What happened in the selected mailbox while the client was not asking (RFC 9051 §7): a message arrived, one was
expunged, UIDs vanished (QRESYNC), a message's flags changed, the mailbox's flags changed, the server's alert, its
goodbye. A client gives every update it reads to the options' `on_update`, and [idle](client/idle.md) returns those
that ended it; [mailbox](client/mailbox.md) has the counts they moved.

## Member objects

| Member | Description |
|---|---|
| `kind` | what it is ([update::kind](update-kind.md)) |
| `number` | exists, recent: the count; expunge, fetch: the message's sequence number |
| `uid` | fetch: the UID, when the server sent it |
| `flags` | fetch: the message's flags; flags: the mailbox's |
| `modseq` | fetch: the mod-sequence, when sent |
| `uids` | vanished: the UIDs |
| `text` | alert, bye: the server's text |

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
    mail.append("alice", "INBOX", "Subject: new\r\n\r\nx\r\n");
    auto updates = session.idle(std::chrono::seconds(5));
    for (const net::imap::update& u : *updates) {
        println("{} {}", u.kind == net::imap::update::kind::exists, u.number);
    }
    srv.close();
}
```

Output:

```text
true 1
```

## See also

- [idle](client/idle.md), [client::options](client-options.md)'s `on_update`
- [update::kind](update-kind.md)
- [sgcl::net::imap](README.md)

[sgcl](../../README.md) › [net](../README.md) › [imap](README.md) › [update](update.md) › kind

# sgcl::net::imap::update::kind

```cpp
#include "sgcl/net/imap/types.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    struct update {
        enum class kind : uint8_t { exists, recent, expunge, vanished, fetch, flags, alert, bye };
    };
}
```

What an [update](update.md) is: which of the server's untagged responses about the selected mailbox it came of (RFC
9051 §7).

| Value | Description |
|---|---|
| `exists` | EXISTS: the number of messages now, a new message arrived; `number` |
| `recent` | RECENT (IMAP4rev1): the number of recent messages; `number` |
| `expunge` | EXPUNGE: a message removed, `number` its sequence number (the numbers after it move down) |
| `vanished` | VANISHED (QRESYNC): the UIDs removed; `uids` |
| `fetch` | FETCH: a message's flags changed; `number`, `uid` when sent, `flags`, `modseq` |
| `flags` | FLAGS: the flags the mailbox uses; `flags` |
| `alert` | an OK [ALERT]: the server's text for the user; `text` |
| `bye` | BYE: the server ends the connection; `text` |

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
    mail.append("alice", "INBOX", "Subject: new\r\n\r\nx\r\n");   // a delivery of the program's
    auto got = session.idle(std::chrono::seconds(5));
    println("{}", (*got)[0].kind == net::imap::update::kind::exists);
    srv.close();
}
```

Output:

```text
true
```

## See also

- [update](update.md)
- [idle](client/idle.md)
- [sgcl::net::imap](README.md)

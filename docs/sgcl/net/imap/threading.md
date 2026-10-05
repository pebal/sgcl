[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::threading

```cpp
#include "sgcl/net/imap/client.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    enum class threading : uint8_t { references, ordered_subject };
}
```

The algorithm of [threads](client/threads.md) (RFC 5256 §3).

| Value | Description |
|---|---|
| `references` | THREAD=REFERENCES: by Message-ID, References and In-Reply-To (Jamie Zawinski's algorithm), then threads of the same base subject joined |
| `ordered_subject` | THREAD=ORDEREDSUBJECT: by base subject alone, the first of each the parent of the rest |

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
    mail.append("alice", "INBOX", "Subject: plan\r\nDate: Mon, 5 Oct 2026 10:00:00 +0000\r\n\r\nx\r\n");
    mail.append("alice", "INBOX", "Subject: Re: plan\r\nDate: Mon, 5 Oct 2026 11:00:00 +0000\r\n\r\ny\r\n");
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
    auto t = session.threads(net::imap::criteria(), net::imap::threading::ordered_subject);
    println("{} thread, {} reply", t->size(), (*t)[0].children.size());
    srv.close();
}
```

Output:

```text
1 thread, 1 reply
```

## See also

- [threads](client/threads.md), [thread](thread.md)
- [sgcl::net::imap](README.md)

[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::thread

```cpp
#include "sgcl/net/imap/types.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    struct thread {
        uint32_t uid = 0;
        vector<thread> children;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

A thread as THREAD gives it (RFC 5256): a message (its UID) and the threads under it, the replies. A UID of 0 is a
parent the algorithm put in for messages whose own parent is not in the mailbox, as RFC 5256 has it.

## Member objects

| Member | Description |
|---|---|
| `uid` | the message's UID; 0 for a missing parent |
| `children` | the threads under it |

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
    mail.append("alice", "INBOX", "Message-ID: <a@x>\r\nSubject: plan\r\n\r\nx\r\n");
    mail.append("alice", "INBOX", "Message-ID: <b@x>\r\nIn-Reply-To: <a@x>\r\nSubject: Re: plan\r\n\r\ny\r\n");
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
    auto threads = session.threads();
    const net::imap::thread& first = (*threads)[0];
    println("{} -> {}", first.uid, first.children[0].uid);
    srv.close();
}
```

Output:

```text
1 -> 2
```

## See also

- [threads](client/threads.md), [threading](threading.md)
- [sgcl::net::imap](README.md)

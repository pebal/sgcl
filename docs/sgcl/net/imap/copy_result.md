[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::copy_result

```cpp
#include "sgcl/net/imap/types.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    struct copy_result {
        uint32_t uid_validity = 0;
        vector<uint32_t> source;
        vector<uint32_t> destination;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

What the server answered a COPY or a MOVE (UIDPLUS's COPYUID, RFC 4315): the destination's UIDVALIDITY and the UIDs in
pairs, `source[i]` became `destination[i]`. Empty when the server gave no COPYUID.

## Member objects

| Member | Description |
|---|---|
| `uid_validity` | the destination's UIDVALIDITY |
| `source` | the UIDs copied, in the order of the pairs |
| `destination` | the UIDs they got |

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
    session.create("Archive");
    session.select("INBOX");
    net::imap::copy_result r = session.copy(1, "Archive");
    println("{} -> {}", r.source, r.destination);
    srv.close();
}
```

Output:

```text
[1] -> [1]
```

## See also

- [copy](client/copy.md), [move](client/move.md)
- [sgcl::net::imap](README.md)

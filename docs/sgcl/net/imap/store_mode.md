[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::store_mode

```cpp
#include "sgcl/net/imap/client.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    enum class store_mode : uint8_t { replace, add, remove };
}
```

How [store](client/store.md) changes the flags of messages: STORE's `FLAGS`, `+FLAGS` and `-FLAGS`.
[set_flags](client/set_flags.md), [add_flags](client/add_flags.md) and [remove_flags](client/remove_flags.md) are the
three modes in one call each.

| Value | Description |
|---|---|
| `replace` | the flags given become the message's flags |
| `add` | the flags given are added to the message's |
| `remove` | the flags given are taken from the message's |

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
    session.select("INBOX");
    session.store(1, net::imap::store_mode::add, {net::imap::flag::flagged, "$Work"});
    session.store(1, net::imap::store_mode::remove, {"$Work"});
    auto m = session.fetch(1);
    println("{}", (*m)[0].flags);
    srv.close();
}
```

Output:

```text
["\\Flagged"]
```

## See also

- [store](client/store.md)
- [sgcl::net::imap](README.md)

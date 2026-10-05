[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::mailbox

```cpp
selected mailbox() const;
```

Returns what the client knows of the selected mailbox now: what [select](select.md) said, with the counts the server's
updates moved since (EXISTS, EXPUNGE, VANISHED).

## Parameters

None.

## Return value

The state; a default [selected](../selected.md) when no mailbox is selected.

## Complexity

Linear in the size of the state.

## Exceptions

None.

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
    println("{} {}", session.mailbox().name, session.mailbox().exists);
    srv.close();
}
```

Output:

```text
INBOX 1
```

## See also

- [select](select.md), [noop](noop.md), [idle](idle.md)
- [sgcl::net::imap::client](README.md)

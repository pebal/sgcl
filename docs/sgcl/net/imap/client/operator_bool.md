[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::operator bool

```cpp
explicit operator bool() const noexcept;
```

Returns whether the handle holds a connection: `false` for a default-constructed client, `true` for one
[connect](connect.md) made, even after the connection ended ([is_closed](is_closed.md) says that).

## Parameters

None.

## Return value

`true` when the handle holds a connection.

## Complexity

Constant.

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
    net::imap::client none;
    println("{} {}", bool(none), bool(session));
    session.logout();
    println("{} {}", bool(session), session.is_closed());
    srv.close();
}
```

Output:

```text
false true
true true
```

## See also

- [is_closed](is_closed.md)
- [sgcl::net::imap::client](README.md)

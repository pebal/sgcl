[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::client

```cpp
client() noexcept;                       // (1)
client(const client& other) noexcept;    // (2)
```

1. Constructs a client of no connection: [operator bool](operator_bool.md) is `false`, and a command on it is a
   contract violation. A client comes from [connect](connect.md).
2. The same connection as `other`: a client is a handle, its copies one connection. A move is the copy.

## Parameters

| Parameter | Description |
|---|---|
| `other` | another client |

## Return value

None.

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
    net::imap::client same = session;
    println("{} {}", bool(none), bool(same));
    same.noop();
    srv.close();
}
```

Output:

```text
false true
```

## See also

- [connect](connect.md)
- [operator bool](operator_bool.md)
- [sgcl::net::imap::client](README.md)

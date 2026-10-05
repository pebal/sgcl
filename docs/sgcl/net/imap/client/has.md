[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::has

```cpp
bool has(const string& capability) const;
```

Returns whether the server has a capability, in any case.

## Parameters

| Parameter | Description |
|---|---|
| `capability` | `"IDLE"`, `"MOVE"`, `"AUTH=XOAUTH2"` |

## Return value

`true` when the server listed it.

## Complexity

Linear in the number of capabilities.

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
    println("{} {} {}", session.has("idle"), session.has("MOVE"), session.has("X-UNKNOWN"));
    srv.close();
}
```

Output:

```text
true true false
```

## See also

- [capabilities](capabilities.md)
- [sgcl::net::imap::client](README.md)

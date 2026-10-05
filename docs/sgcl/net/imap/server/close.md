[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [server](README.md)

# sgcl::net::imap::server::close

```cpp
void close() const;
```

Ends the server at once: every listener and connection closed, the commands in progress ended.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of connections.

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
    srv.close();
    println("{}", session.noop().has_value());
    srv.close();
}
```

Output:

```text
false
```

## See also

- [shutdown](shutdown.md)
- [sgcl::net::imap::server](README.md)

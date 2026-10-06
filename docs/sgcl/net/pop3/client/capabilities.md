[sgcl](../../../README.md) › [net](../../README.md) › [pop3](../README.md) › [client](README.md)

# sgcl::net::pop3::client::capabilities

```cpp
vector<string> capabilities() const;
```

The lines of CAPA (RFC 2449), as the server sent them when the session started (and again after STLS): `"TOP"`, `"UIDL"`, `"SASL PLAIN"`, `"PIPELINING"`, `"RESP-CODES"`. Empty for a server of RFC 1939 alone, which the client then takes to have TOP and UIDL.

## Parameters

None.

## Return value

The lines.

## Complexity

Linear in them.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/imap.h"
#include "sgcl/net/pop3.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::imap::memory_backend mail;
    mail.add_user("alice", "secret");
    mail.append("alice", "INBOX", "From: bob@example.com\r\nSubject: Lunch\r\n\r\nNoon?\r\n");
    mail.append("alice", "INBOX", "From: carol@example.com\r\nSubject: Report\r\n\r\nLine 1\r\nLine 2\r\n");
    net::pop3::server srv;
    srv.backend = mail;
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    net::pop3::client::options o;
    o.user = "alice";
    o.password = "secret";
    o.security = net::pop3::security::none;  // the loopback, no TLS
    net::pop3::client c = net::pop3::client::connect(l.local_endpoint().to_string(), o).value();
    println("{}", c.capabilities()[0]);
    c.quit();
    srv.close();
    serving.wait();
}
```

Output:

```text
TOP
```

## See also

- [has](has.md)
- [client](README.md)

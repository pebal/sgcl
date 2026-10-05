[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::capabilities

```cpp
vector<string> capabilities() const;
```

Returns the server's capabilities as the client knows them now, upper-cased: those of the greeting, read again after
STARTTLS and after the login, as RFC 9051 asks.

## Parameters

None.

## Return value

The capabilities: `"IMAP4REV1"`, `"IMAP4REV2"`, `"IDLE"`, `"AUTH=PLAIN"` ...

## Complexity

Linear in their number.

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
    auto caps = session.capabilities();
    println("{} {}", caps[0], caps[1]);
    srv.close();
}
```

Output:

```text
IMAP4REV1 IMAP4REV2
```

## See also

- [has](has.md)
- [sgcl::net::imap::client](README.md)

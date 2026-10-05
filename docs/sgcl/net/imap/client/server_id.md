[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::server_id

```cpp
vector<pair<string, string>> server_id() const;
```

Returns the server's answer to ID (RFC 2971), sent at the connect when the options' `id` was not empty.

## Parameters

None.

## Return value

The server's fields in their order; empty when ID was not sent or the server answered NIL.

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
    o.id = {{"name", "docs"}, {"version", "1.0"}};
    string address = "127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::imap::client session = net::imap::client::connect(address, o);
    for (const auto& [field, value] : session.server_id()) {
        println("{} = {}", field, value);
    }
    srv.close();
}
```

Output:

```text
name = sgcl
```

## See also

- [client::options](../client-options.md)
- [sgcl::net::imap::client](README.md)

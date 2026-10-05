[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [message](README.md)

# sgcl::net::imap::message::text

```cpp
string text() const noexcept;
```

Returns the whole message, the section `""` (BODY[]): what [fetch_message](../client/fetch_message.md) gives of one
UID.

## Parameters

None.

## Return value

The message; empty when it was not fetched.

## Complexity

Linear in the number of sections fetched.

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
    net::imap::fetch_options what;
    what.sections = {""};
    auto messages = session.fetch(1, what);
    print("{}", (*messages)[0].text());
    srv.close();
}
```

Output:

```text
From: Bob <bob@example.com>
Subject: Lunch

Noon?
```

## See also

- [section](section.md)
- [fetch_message](../client/fetch_message.md)
- [message](README.md)

[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [message](README.md)

# sgcl::net::imap::message::email

```cpp
expected<encoding::email, encoding::error> email() const;                                    // (1)
expected<encoding::email, encoding::error> email(const encoding::email::limits& l) const;    // (2)
```

Returns the whole message (the section `""`, as [text](text.md) gives it) parsed, as
[encoding::email::parse](../../../encoding/email/parse.md) parses one: (1) within the default limits, (2) within `l`.
[fetch_unseen](../client/fetch_unseen.md) fetches the whole of each message, so its messages parse as they come.

## Parameters

| Parameter | Description |
|---|---|
| `l` | the limits |

## Return value

The [email](../../../encoding/email/README.md), or the [error](../../../encoding/error/README.md) of a limit. A
message whose whole was not fetched parses as an empty one, without fields or text.

## Complexity

Linear in the size of the message.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
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
    auto unseen = session.fetch_unseen();
    for (const net::imap::message& m : *unseen) {
        auto mail = m.email();
        println("{} | {} | {}", mail->from()->addr(), mail->subject(), mail->text().trim());
    }
    srv.close();
}
```

Output:

```text
bob@example.com | Lunch | Noon?
```

## See also

- [text](text.md)
- [fetch_unseen](../client/fetch_unseen.md)
- [encoding::email](../../../encoding/email/README.md)
- [message](README.md)

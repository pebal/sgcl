[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::order

```cpp
#include "sgcl/net/imap/client.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    enum class order : uint8_t { arrival, cc, date, from, size, subject, to, display_from, display_to };
}
```

What [sort](client/sort.md) orders by (RFC 5256 §3, RFC 5957's display keys): the server compares the messages by the
first key, then the next, ties by their arrival.

| Value | Description |
|---|---|
| `arrival` | INTERNALDATE: when the server received the message |
| `cc` | the mailbox (the part before "@") of the first Cc address |
| `date` | the Date: field, the instant (the internal date where there is none) |
| `from` | the mailbox of the first From address |
| `size` | RFC822.SIZE |
| `subject` | the base subject ("Re:", "Fwd:", "[list]" taken off, RFC 5256 §2.1) |
| `to` | the mailbox of the first To address |
| `display_from` | the display name of the first From address, else its address (SORT=DISPLAY) |
| `display_to` | the same of the first To address |

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
    mail.append("alice", "INBOX", "Subject: Zebra\r\n\r\nz\r\n");
    mail.append("alice", "INBOX", "Subject: Re: apple\r\n\r\na\r\n");
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
    auto by_subject = session.sort({net::imap::order::subject});
    println("{}", *by_subject);
    srv.close();
}
```

Output:

```text
[2, 1]
```

## See also

- [sort](client/sort.md)
- [sgcl::net::imap](README.md)

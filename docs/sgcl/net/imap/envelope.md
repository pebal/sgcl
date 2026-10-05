[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::envelope

```cpp
#include "sgcl/net/imap/types.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    struct envelope {
        string date;
        string subject;
        vector<address> from;
        vector<address> sender;
        vector<address> reply_to;
        vector<address> to;
        vector<address> cc;
        vector<address> bcc;
        string in_reply_to;
        string message_id;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

The envelope of a message (RFC 9051 §7.5.2): the fields of its header the server parsed, what a list of messages shows
without the message itself. A [message](message/README.md) has one when [fetch](client/fetch.md) asked for it
(`envelope` of [fetch_options](fetch_options.md)), and a message/rfc822 part of a
[body_structure](body_structure/README.md) has the envelope of the message it holds. The subject and the names are
decoded from RFC 2047 into UTF-8; the date and the ids are as written. Sender and Reply-To are From's when the message
has none, as the server gives them.

## Member objects

| Member | Description |
|---|---|
| `date` | the Date: field as written (`Mon, 5 Oct 2026 10:00:00 +0200`); empty when there is none |
| `subject` | the Subject:, decoded |
| `from`, `sender`, `reply_to`, `to`, `cc`, `bcc` | the [addresses](address/README.md) of those fields, groups flattened |
| `in_reply_to` | In-Reply-To: as written (`<id@host>`) |
| `message_id` | Message-ID: as written |

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
    mail.append("alice", "INBOX", "From: Bob <bob@example.com>\r\nSubject: =?UTF-8?Q?Cze=C5=9B=C4=87?=\r\n"
                              "Message-ID: <1@example.com>\r\n\r\nx\r\n");
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
    what.envelope = true;
    auto messages = session.fetch(1, what);
    const net::imap::envelope& e = *(*messages)[0].envelope;
    println("{} | {} | {}", e.subject, e.from[0].email(), e.message_id);
    println("{}", e.reply_to[0].email());
    srv.close();
}
```

Output:

```text
Cześć | bob@example.com | <1@example.com>
bob@example.com
```

## See also

- [message](message/README.md), [fetch_options](fetch_options.md)
- [address](address/README.md)
- [sgcl::net::imap](README.md)

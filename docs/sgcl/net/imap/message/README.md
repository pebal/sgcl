[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md)

# sgcl::net::imap::message

```cpp
#include "sgcl/net/imap/types.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    struct message {
        uint32_t seq = 0;
        uint32_t uid = 0;
        vector<string> flags;
        optional<time::datetime> internal_date;
        uint64_t size = 0;
        uint64_t modseq = 0;
        optional<imap::envelope> envelope;
        optional<imap::body_structure> body_structure;
        vector<pair<string, string>> sections;
        vector<pair<string, uint64_t>> binary_sizes;

        optional<string> section(const string& part) const noexcept;
        string text() const noexcept;
        expected<encoding::email, encoding::error> email() const;
        expected<encoding::email, encoding::error> email(const encoding::email::limits& l) const;
        bool has_flag(const string& f) const noexcept;
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

A message as [fetch](../client/fetch.md) gave it: the items [fetch_options](../fetch_options.md) asked for, the UID
always, the rest left at their defaults. A section's bytes are as the server sent them, a whole message (`""`) its
octets with CRLF line ends; with `binary` on, decoded from the part's transfer encoding.

## Member objects

| Member | Description |
|---|---|
| `seq` | the message's sequence number when it was fetched |
| `uid` | its UID |
| `flags` | `\Seen`, `\Flagged`, `$Forwarded` ...; `flags` of the options, on by default |
| `internal_date` | INTERNALDATE: when the server received it, in the zone it gave |
| `size` | RFC822.SIZE: the octets of the whole message |
| `modseq` | its mod-sequence (CONDSTORE) |
| `envelope`, `body_structure` | the [envelope](../envelope.md) and the [structure](../body_structure/README.md) |
| `sections` | each section fetched (`""`, `"HEADER"`, `"TEXT"`, `"1.2"`) and its bytes |
| `binary_sizes` | BINARY.SIZE of sections |

## Member functions

| Function | Description |
|---|---|
| [section](section.md) | the bytes of a section fetched |
| [text](text.md) | the whole message, when it was fetched |
| [email](email.md) | the whole message parsed: header fields, text, HTML, attachments |
| [has_flag](has_flag.md) | whether a flag is set |

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
    what.size = true;
    what.sections = {"HEADER.FIELDS (SUBJECT)", "TEXT"};
    auto messages = session.fetch(net::imap::sequence_set::all(), what);
    for (const net::imap::message& m : *messages) {
        print("{} {} {}", m.uid, m.size, *m.section("TEXT"));
    }
    srv.close();
}
```

Output:

```text
1 54 Noon?
```

## See also

- [fetch](../client/fetch.md), [fetch_options](../fetch_options.md)
- [sgcl::net::imap](../README.md)

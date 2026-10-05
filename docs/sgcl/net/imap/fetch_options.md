[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::fetch_options

```cpp
#include "sgcl/net/imap/client.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    struct fetch_options {
        bool flags = true;
        bool envelope = false;
        bool body_structure = false;
        bool size = false;
        bool internal_date = false;
        bool modseq = false;
        vector<string> sections;
        bool binary = false;
        bool mark_seen = false;
        uint64_t partial_offset = 0;
        uint64_t partial_length = 0;
        uint64_t changed_since = 0;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

What [fetch](client/fetch.md) asks for of each message, beside its UID, which it always does. The defaults ask for the
flags alone; a section is fetched by BODY.PEEK, which leaves `\Seen` as it is, unless `mark_seen`.

## Member objects

| Member | Description |
|---|---|
| `flags` | FLAGS; `true` by default |
| `envelope` | ENVELOPE |
| `body_structure` | BODYSTRUCTURE |
| `size` | RFC822.SIZE |
| `internal_date` | INTERNALDATE |
| `modseq` | MODSEQ (CONDSTORE) |
| `sections` | the sections: `""` the whole message, `"HEADER"`, `"TEXT"`, `"1.2"`, `"HEADER.FIELDS (FROM TO)"`, `"2.MIME"` |
| `binary` | the sections as BINARY (RFC 3516): decoded from their transfer encoding by the server, when it has BINARY |
| `mark_seen` | BODY instead of BODY.PEEK: the messages read are marked `\Seen` |
| `partial_offset`, `partial_length` | `<offset.length>` of every section; a length of 0 is the whole section |
| `changed_since` | CHANGEDSINCE (CONDSTORE): only the messages whose mod-sequence is past it |

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
    what.sections = {"TEXT"};
    what.partial_length = 4;
    auto messages = session.fetch(1, what);
    println("{}", *(*messages)[0].section("TEXT"));
    srv.close();
}
```

Output:

```text
Noon
```

## See also

- [fetch](client/fetch.md), [message](message/README.md)
- [sgcl::net::imap](README.md)

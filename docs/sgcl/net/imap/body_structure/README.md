[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md)

# sgcl::net::imap::body_structure

```cpp
#include "sgcl/net/imap/types.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    struct body_structure {
        string type;
        string subtype;
        vector<pair<string, string>> parameters;
        string id;
        string description;
        string encoding;
        uint64_t size = 0;
        uint64_t lines = 0;
        string md5;
        string disposition;
        vector<pair<string, string>> disposition_parameters;
        vector<string> language;
        string location;
        vector<body_structure> parts;
        optional<imap::envelope> envelope;
        string part;

        string parameter(const string& name) const noexcept;
        bool is_multipart() const noexcept;
        string filename() const noexcept;
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

The MIME structure of a message, BODYSTRUCTURE (RFC 9051 §7.5.2): a part with its type, parameters, encoding and size,
the parts of a multipart, and for a message/rfc822 part the envelope and the structure of the message it holds, its
one part. `part` is the section number to fetch the part by: [fetch](../client/fetch.md) with `sections = {"2"}` gives
the bytes of part 2, and with `binary` on, the bytes decoded from their transfer encoding. Types, subtypes, encodings
and dispositions are in lower case, parameters' names too; the names of attachments (RFC 2231 and RFC 2047) are
decoded.

## Member objects

| Member | Description |
|---|---|
| `type`, `subtype` | `text` and `plain`, `multipart` and `mixed`, `image` and `png` ... |
| `parameters` | Content-Type's parameters (`charset`, `boundary`, `name`), the names in lower case |
| `id`, `description` | Content-ID and Content-Description |
| `encoding` | Content-Transfer-Encoding: `7bit`, `8bit`, `binary`, `base64`, `quoted-printable` |
| `size` | the octets of the part's body as it is encoded |
| `lines` | the lines of a text or message/rfc822 part's body |
| `md5` | Content-MD5 |
| `disposition`, `disposition_parameters` | Content-Disposition: `attachment` or `inline`, and its `filename` and the rest |
| `language`, `location` | Content-Language and Content-Location |
| `parts` | a multipart's parts; a message/rfc822 part's message |
| `envelope` | a message/rfc822 part's envelope |
| `part` | the section number: `1`, `2.1`; empty for a multipart message itself |

## Member functions

| Function | Description |
|---|---|
| [parameter](parameter.md) | a parameter of Content-Type by its name |
| [is_multipart](is_multipart.md) | whether the part has parts |
| [filename](filename.md) | the file name of an attachment |

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
    mail.append("alice", "INBOX", "Subject: report\r\nContent-Type: multipart/mixed; boundary=b\r\n\r\n"
                              "--b\r\nContent-Type: text/plain\r\n\r\nSee attached.\r\n"
                              "--b\r\nContent-Type: application/pdf; name=q3.pdf\r\n"
                              "Content-Transfer-Encoding: base64\r\n\r\nJVBERi0x\r\n--b--\r\n");
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
    what.body_structure = true;
    auto messages = session.fetch(1, what);
    for (const auto& p : (*messages)[0].body_structure->parts) {
        println("{} {}/{} {} bytes {}", p.part, p.type, p.subtype, p.size, p.filename());
    }
    srv.close();
}
```

Output:

```text
1 text/plain 13 bytes 
2 application/pdf 8 bytes q3.pdf
```

## See also

- [message](../message/README.md), [fetch_options](../fetch_options.md)
- [sgcl::net::imap](../README.md)

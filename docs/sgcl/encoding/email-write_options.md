[sgcl](../README.md) › [encoding](README.md) › [email](email/README.md) › write_options

# sgcl::encoding::email::write_options

```cpp
#include "sgcl/encoding/email.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class email {
    public:
        struct write_options {
            bool allow_8bit = false;
            bool allow_utf8 = false;
            bool write_bcc = true;
        };
    };
}
```

`sgcl::encoding::email::write_options` is how a message is written: for a transport that takes 8bit content
(SMTP's 8BITMIME, RFC 6152), one that takes UTF-8 in the head (SMTPUTF8, RFC 6532), and whether the Bcc field goes
with it. [net::smtp](../net/smtp/README.md) sets them from what the server offers. A plain struct, its fields set
by name; the forms without it write for 7 bits, the Bcc included.

## Member objects

| Member | Description |
|---|---|
| `allow_8bit` | text past ASCII written as it is (Content-Transfer-Encoding: 8bit) where its lines allow; `false` by default: quoted-printable or base64 |
| `allow_utf8` | the head's text past ASCII written as UTF-8, addresses and domains included; `false` by default: encoded words, IDNA |
| `write_bcc` | the Bcc field written; `true` by default |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    encoding::email m("Łucja <lucja@example.pl>", "b@example.com", "Zażółć", "x");
    encoding::email::write_options o;
    o.allow_utf8 = true;
    auto text = m.to_string(o);
    println("{}", text.view().find("Subject: Zażółć") != std::string_view::npos);
    println("{}", m.to_string().view().find("Subject: =?utf-8?b?") != std::string_view::npos);
}
```

Output:

```text
true
true
```

## See also

- [to_string](email/to_string.md), [write_to](email/write_to.md), [save](email/save.md): what take them
- [email](email/README.md)

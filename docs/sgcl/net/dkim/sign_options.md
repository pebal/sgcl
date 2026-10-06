[sgcl](../../README.md) › [net](../README.md) › [dkim](README.md) › sign_options

# sgcl::net::dkim::sign_options

```cpp
#include "sgcl/net/dkim.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::dkim {
    struct sign_options {
        canonicalization header = canonicalization::relaxed;
        canonicalization body = canonicalization::relaxed;
        vector<string> headers;
        bool oversign = true;
        string identity;
        optional<uint64_t> body_length;
        optional<time::datetime> time;
        duration expiration = {};
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::dkim::sign_options` is how [sign](signer/sign.md) signs: the canonicalizations, the fields covered, and the tags
of the signature a sender may want — the identity, the body's length, the time and the expiration. The defaults are
what senders use: relaxed both ways, the fields RFC 6376 recommends, each oversigned.

## Member objects

| Member | Description |
|---|---|
| `header` | the head's [canonicalization](canonicalization.md); `relaxed` by default |
| `body` | the body's; `relaxed` by default |
| `headers` | the names of the fields signed, From first whatever the list holds; a name the message lacks is signed once (no such field may be added); empty by default: those of RFC 6376 §5.4.1 the message has, with Message-ID, MIME-Version and the content fields |
| `oversign` | each name signed once more than the message has it, so that no field of the name can be added after (RFC 6376 §8.15); `true` by default |
| `identity` | `i=`, the user or agent the domain signs for (`"alice@example.com"`, `"@mail.example.com"`), within the signer's domain; empty by default: none written |
| `body_length` | `l=`: only the first bytes of the canonical body signed, so that a list may add a footer; none by default (RFC 6376 §8.2 advises against it: what follows can be anything) |
| `time` | `t=`; none by default: now |
| `expiration` | `x=` is `t=` and this; zero by default: no expiration |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/time.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    auto s = net::dkim::signer::generate("example.com", "s1", net::dkim::algorithm::ed25519_sha256);
    net::dkim::sign_options o;
    o.headers = {"Subject"};
    o.oversign = false;
    o.identity = "alice@example.com";
    o.time = time::datetime::from_unix(1791244800, time::zone::utc());
    o.expiration = 7 * 24h;
    string m = s.sign("From: alice@example.com\r\nSubject: Hi\r\n\r\nHello\r\n", o).value();
    println("{}", m.view().substr(0, m.find("bh=")));
}
```

Output:

```text
DKIM-Signature: v=1; a=ed25519-sha256; c=relaxed/relaxed; d=example.com;
	s=s1; t=1791244800; x=1791849600; i=alice@example.com; h=from:
	subject; 
```

## See also

- [sign](signer/sign.md)
- [dkim](README.md)

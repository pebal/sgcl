[sgcl](../../README.md) › [net](../README.md) › [dkim](README.md) › result

# sgcl::net::dkim::result

```cpp
#include "sgcl/net/dkim.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::dkim {
    struct result {
        dkim::status status = dkim::status::none;
        string domain;
        string selector;
        string identity;
        string algorithm;
        string signature;
        string reason;
        bool testing = false;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::dkim::result` is one signature's verdict, what [verify](verify.md) gives for each `DKIM-Signature` field:
the [status](status.md), what the signature names, and why it did not pass. The fields the signature names are
filled as far as it could be read, so that a report says whose signature failed.

## Member objects

| Member | Description |
|---|---|
| `status` | the verdict: `pass`, `fail`, `permerror`, `temperror` |
| `domain` | `d=`, in lower case: the signing domain |
| `selector` | `s=`, in lower case |
| `identity` | `i=`, or `@` and the domain when the signature has none |
| `algorithm` | `a=` as the signature names it: `"rsa-sha256"`, `"ed25519-sha256"` |
| `signature` | `b=` without its whitespace; Authentication-Results gives its first eight characters as `header.b` |
| `reason` | why it did not pass: `"body hash did not verify"`, `"signature did not verify"`, `"no key for signature"`, `"key revoked"`, `"signature expired"`, ...; empty for a pass |
| `testing` | the key's record says `t=y`: the domain is testing DKIM, and asks that a failure be treated as no signature |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    string m = "DKIM-Signature: v=1; a=rsa-sha1; d=example.com; s=old; h=from; bh=AAAA; b=AAAA\r\n"
               "From: alice@example.com\r\n\r\nHello.\r\n";
    net::dkim::result r = net::dkim::verify(m)[0];
    println("{} {} {}", net::dkim::to_string(r.status), r.domain, r.algorithm);
    println("{}", r.reason);
}
```

Output:

```text
permerror example.com rsa-sha1
rsa-sha1 is not accepted (RFC 8301)
```

## See also

- [verify](verify.md)
- [status](status.md)
- [dkim](README.md)

[sgcl](../../README.md) › [net](../README.md) › [dkim](README.md) › verify_options

# sgcl::net::dkim::verify_options

```cpp
#include "sgcl/net/dkim.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::dkim {
    struct verify_options {
        net::dns::options dns;
        size_t max_signatures = 5;
        optional<time::datetime> at;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::dkim::verify_options` is how [verify](verify.md) checks: where it asks for the keys, how many signatures
it checks (each one a DNS lookup, so a message of hundreds of signatures cannot make a receiver ask hundreds of
times), and the time an expiration is compared against.

## Member objects

| Member | Description |
|---|---|
| `dns` | the resolver of the key records ([dns::options](../dns-options.md)): its servers empty, `/etc/resolv.conf`'s |
| `max_signatures` | the first signatures of the head checked, the rest ignored; 5 by default |
| `at` | the time `x=` is compared against; none by default: now |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    string m = "DKIM-Signature: v=1; a=ed25519-sha256; d=example.com; s=s1; h=from; t=1000; x=2000; "
               "bh=AAAA; b=AAAA\r\nFrom: alice@example.com\r\n\r\nHi\r\n";
    net::dkim::verify_options o;
    o.at = time::datetime::from_unix(3000, time::zone::utc());
    println("{}", net::dkim::verify(m, o)[0].reason);
}
```

Output:

```text
signature expired
```

## See also

- [verify](verify.md)
- [dns::options](../dns-options.md)
- [dkim](README.md)

[sgcl](../../README.md) › [net](../README.md) › [dmarc](README.md) › options

# sgcl::net::dmarc::options

```cpp
#include "sgcl/net/dmarc.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::dmarc {
    struct options {
        net::dns::options dns;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::dmarc::options` is how [check](check.md) and [lookup](lookup.md) ask for the records: the resolver.

## Member objects

| Member | Description |
|---|---|
| `dns` | the resolver ([dns::options](../dns-options.md)): its servers empty, `/etc/resolv.conf`'s |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    net::dmarc::options o;
    o.dns.servers = {"9.9.9.9"};
    auto r = net::dmarc::lookup("example.com", o);
    println("{}", r ? r->to_string() : r.error().message());
}
```

Sample output:

```text
v=DMARC1; p=reject; sp=reject; adkim=s; aspf=s
```

## See also

- [check](check.md), [lookup](lookup.md)
- [dmarc](README.md)

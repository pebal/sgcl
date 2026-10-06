[sgcl](../../README.md) › [net](../README.md) › [spf](README.md) › options

# sgcl::net::spf::options

```cpp
#include "sgcl/net/spf.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::spf {
    struct options {
        net::dns::options dns;
        duration timeout = 20 * second;
        size_t max_lookups = 10;
        size_t max_void_lookups = 2;
        string receiver;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::spf::options` is how [check](check.md) asks and what it allows: the resolver, the time of the whole
check, and the limits of RFC 7208 §4.6.4, which keep a record from making a receiver ask DNS without end (or ask it
on someone else's behalf). The defaults are the RFC's.

## Member objects

| Member | Description |
|---|---|
| `dns` | the resolver ([dns::options](../dns-options.md)): its servers empty, `/etc/resolv.conf`'s |
| `timeout` | the whole check; past it `temperror`; 20 s by default (§4.6.4: at least 20 seconds) |
| `max_lookups` | the terms that ask DNS (`include`, `a`, `mx`, `ptr`, `exists`, `redirect`, and `%{p}`); past it `permerror`; 10 by default |
| `max_void_lookups` | the lookups that find no name or no record; past it `permerror`; 2 by default |
| `receiver` | the receiving host's name, `%{r}` of an explanation; empty by default: `"unknown"` |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    net::spf::options o;
    o.dns.servers = {"1.1.1.1"};
    o.timeout = 5s;
    auto r = net::spf::check(net::ip_address("192.0.2.25"), "alice@example.com", "mail.example.com", o);
    println("{}", net::spf::to_string(r.status));
}
```

Sample output:

```text
fail
```

## See also

- [check](check.md)
- [dns::options](../dns-options.md)
- [spf](README.md)

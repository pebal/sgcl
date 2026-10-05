[sgcl](../README.md) › [net](README.md) › [dns](dns/README.md) › mx

# sgcl::net::dns::mx

```cpp
#include "sgcl/net/dns.h"   // or "sgcl/net.h"

namespace sgcl::net {
    struct dns {
        struct mx {
            string host;
            uint16_t preference = 0;

            friend bool operator==(const mx&, const mx&) noexcept = default;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::dns::mx` is a mail exchanger of a domain, what [lookup_mx](dns/lookup_mx.md) gives one of: the host that
takes the domain's mail and its preference, the lower the sooner it is tried (RFC 1035 §3.3.9, RFC 5321 §5.1). Go's
`net.MX`, with the host a [string](../core/string/README.md) and the preference named in full. A plain struct of
two fields, compared field by field.

## Member objects

| Member | Description |
|---|---|
| `host` | the exchanger's name, absolute, with its trailing dot (`"mx1.example.com."`); `"."` with preference 0 is RFC 7505's null MX, a domain that takes no mail; empty by default |
| `preference` | the order of the exchangers, the lowest first; `0` by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::dns::mx primary{"mx1.example.com.", 10};
    net::dns::mx none{".", 0};
    println("{} {}", primary.preference, primary.host);
    println("{}", primary == net::dns::mx{"mx1.example.com.", 10});
    println("{}", none.host == "." && none.preference == 0);
}
```

Output:

```text
10 mx1.example.com.
true
true
```

## See also

- [lookup_mx, async_lookup_mx](dns/lookup_mx.md): what gives it
- [dns::srv](dns-srv.md): the servers of other services
- [sgcl::net::dns](dns/README.md)

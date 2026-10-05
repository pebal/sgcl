[sgcl](../README.md) › [net](README.md) › [dns](dns/README.md) › srv

# sgcl::net::dns::srv

```cpp
#include "sgcl/net/dns.h"   // or "sgcl/net.h"

namespace sgcl::net {
    struct dns {
        struct srv {
            string target;
            uint16_t port = 0;
            uint16_t priority = 0;
            uint16_t weight = 0;

            friend bool operator==(const srv&, const srv&) noexcept = default;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::dns::srv` is a server of a service (RFC 2782), what [lookup_srv](dns/lookup_srv.md) gives one of: the host
and port that offer the service, the priority among them, the lower the sooner it is tried, and the weight that
shares the load among the ones of one priority. Go's `net.SRV`. A plain struct of four fields, compared field by
field.

## Member objects

| Member | Description |
|---|---|
| `target` | the server's name, absolute, with its trailing dot (`"sip1.example.com."`); `"."` says the service is not offered at the domain; empty by default |
| `port` | the service's port on the target; `0` by default |
| `priority` | the order of the servers, the lowest first; `0` by default |
| `weight` | among the servers of one priority, the share of the clients each is to take first; `0` by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::dns::srv s{"xmpp1.example.com.", 5269, 5, 50};
    println("{}:{} {} {}", s.target, s.port, s.priority, s.weight);
    println("{}", s == net::dns::srv{"xmpp1.example.com.", 5269, 5, 50});
    println("{}", s == net::dns::srv{"xmpp1.example.com.", 5269, 5, 0});
}
```

Output:

```text
xmpp1.example.com.:5269 5 50
true
false
```

## See also

- [lookup_srv, async_lookup_srv](dns/lookup_srv.md): what gives it, in the RFC's order
- [dns::mx](dns-mx.md): the servers of mail
- [sgcl::net::dns](dns/README.md)

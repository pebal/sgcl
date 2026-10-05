[sgcl](../README.md) › [net](README.md) › [mdns](mdns/README.md) › options

# sgcl::net::mdns::options

```cpp
#include "sgcl/net/mdns.h"   // or "sgcl/net.h"

namespace sgcl::net {
    struct mdns {
        struct options {
            vector<network_interface> interfaces;
            bool ipv4 = true;
            bool ipv6 = true;
            duration timeout = {};
            bool unicast_response = false;
            string host;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::mdns::options` is where and how multicast DNS works for a call: the interfaces and families its sockets
join, how long a one-shot query waits, whether its first question asks for unicast answers, and a responder's name.
It is the last argument of [mdns::lookup](mdns/lookup.md), [responder::start](mdns-responder/start.md) and the
functions of [dns_sd](dns_sd/README.md). A plain struct; what is left at its default is every interface, both
families, two seconds, multicast answers, this machine's name.

## Member objects

| Member | Description |
|---|---|
| `interfaces` | the interfaces joined, each with its own socket per family ([network_interface](network_interface.md), from [interfaces](interfaces.md)); empty by default: every interface up and able to multicast that has an address, the loopback only when there is no other |
| `ipv4` | `224.0.0.251` on IPv4 sockets; true by default |
| `ipv6` | `ff02::fb` on IPv6 sockets, on an interface with an IPv6 address; true by default |
| `timeout` | the wait of a lookup, a resolve, the types: zero or less, the default, 2 s |
| `unicast_response` | the first question of a one-shot query asks for unicast answers (QU, RFC 6762 §5.4), the next ones for multicast; false by default |
| `host` | a responder's host name, its label with `.local` added (`"printer"` or `"printer.local"`); empty by default: this machine's (the first label of `gethostname`), its characters outside letters, digits and `-` made `-` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    net::mdns::options o;
    auto all = net::interfaces();
    for (auto& i : *all) {
        if (i.loopback) {
            o.interfaces.push_back(i);
        }
    }
    o.ipv6 = false;          // IPv4 alone
    o.timeout = 300ms;
    o.host = "docs-options";
    net::mdns::responder r = net::mdns::responder::start(o).value();
    auto found = net::mdns::lookup("docs-options", o);
    println("{}", found->size());  // its IPv6 addresses come with the IPv4 one
    println("{}", net::mdns::lookup("nobody-here", o).error().message());
    r.close();
}
```

Sample output:

```text
3
lookup nobody-here: no such host
```

## See also

- [mdns::lookup](mdns/lookup.md), [responder::start](mdns-responder/start.md), [dns_sd](dns_sd/README.md): what takes it
- [sgcl::net::mdns](mdns/README.md)

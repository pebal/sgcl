[sgcl](../README.md) › [net](README.md)

# sgcl::net::network_interface

```cpp
#include "sgcl/net/interface.h"   // or "sgcl/net.h"

namespace sgcl::net {
    struct network_interface {
        string name;
        uint32_t index = 0;
        vector<ip_network> addresses;
        bool up = false;
        bool loopback = false;
        bool multicast = false;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::network_interface` is a network interface of this machine as [interfaces](interfaces.md) found it: its
name, its index, its addresses and three of its flags. Go's `net.Interface` with the result of its `Addrs()`; the name
is not `interface`, which the headers of Windows define as a macro. A plain struct, a snapshot: an interface that
goes down or gets an address later is not seen in a value taken before.

The index is what the system names the interface by in its calls: the zone of an IPv6 address on it (`fe80::1%4`
names it as `%en0` does), the interface a multicast group is joined on ([join_group](udp-socket/join_group.md)).

## Member objects

| Member | Description |
|---|---|
| `name` | the system's name, `"en0"`, `"eth0"`, `"lo0"`, `"lo"`; empty by default |
| `index` | its index, from 1 (`if_nametoindex`); `0` by default, which the multicast calls take as the system's choice |
| `addresses` | its IPv4 and IPv6 addresses, each with the length of its network's prefix (`192.168.1.20/24`), in the system's order, without a zone; empty by default |
| `up` | the interface is up (`IFF_UP`); `false` by default |
| `loopback` | it is a loopback interface (`IFF_LOOPBACK`); `false` by default |
| `multicast` | it takes multicast (`IFF_MULTICAST`); `false` by default |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    vector<net::network_interface> all = net::interfaces().value();
    for (auto& i : all) {
        if (i.loopback) {
            bool has127 = false;
            for (auto& a : i.addresses) {
                has127 = has127 || a == net::ip_network("127.0.0.1/8");
            }
            println("up {} multicast {} 127.0.0.1/8 {}", i.up, i.multicast, has127);
        }
    }
}
```

Output:

```text
up true multicast true 127.0.0.1/8 true
```

## See also

- [interfaces](interfaces.md), [interface_by_name](interface_by_name.md), [interface_by_index](interface_by_index.md):
  what gives one
- [ip_network](ip_network/README.md): an address and its prefix length
- [join_group](udp-socket/join_group.md): a group joined on an interface

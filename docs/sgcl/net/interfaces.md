[sgcl](../README.md) › [net](README.md)

# sgcl::net::interfaces

```cpp
#include "sgcl/net/interface.h"   // or "sgcl/net.h"

namespace sgcl::net {
    expected<vector<network_interface>, io::error> interfaces() noexcept;
}
```

Returns the network interfaces of this machine, each once, in the system's order: Go's `net.Interfaces` with each
one's `Addrs`. They come from `getifaddrs`, which gives an entry per address; the entries of one name are gathered
into one [network_interface](network_interface.md), its index from `if_nametoindex`. An interface without an IP
address (one that is down, a tunnel not configured) is listed with none.

## Parameters

None.

## Return value

The interfaces; or the [io::error](../io/error/README.md), its operation `interfaces`, with the `errno` of
`getifaddrs`.

## Complexity

One call of `getifaddrs`, linear in its entries, and one `if_nametoindex` per interface.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    vector<net::network_interface> all = net::interfaces().value();
    for (auto& i : all) {
        if (i.up && !i.addresses.empty()) {
            print("{} (index {}):", i.name, i.index);
            for (auto& a : i.addresses) {
                print(" {}", a);
            }
            println("");
        }
    }
}
```

## See also

- [network_interface](network_interface.md): what it lists
- [interface_by_name](interface_by_name.md), [interface_by_index](interface_by_index.md): one of them
- [udp::listen_multicast](udp/listen_multicast.md): a group joined on one

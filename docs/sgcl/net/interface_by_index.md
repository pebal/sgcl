[sgcl](../README.md) › [net](README.md)

# sgcl::net::interface_by_index

```cpp
#include "sgcl/net/interface.h"   // or "sgcl/net.h"

namespace sgcl::net {
    expected<network_interface, io::error> interface_by_index(uint32_t index) noexcept;
}
```

Returns the network interface of this machine whose index is `index`: Go's `net.InterfaceByIndex`. The index is the
one an IPv6 address's numeric zone names (`fe80::1%4`) and the multicast calls take.

## Parameters

| Parameter | Description |
|---|---|
| `index` | the interface's index, from 1 |

## Return value

The [network_interface](network_interface.md); or the [io::error](../io/error/README.md), its operation `interface`
and its path the index: `ENXIO` for an index no interface has, 0 among them; the `errno` of `getifaddrs`.

## Complexity

What [interfaces](interfaces.md) costs: the whole list is read.

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
    net::network_interface first = all.front();
    println("{}", net::interface_by_index(first.index)->name == first.name);
    println("{}", net::interface_by_index(0).error().code() == std::errc::no_such_device_or_address);
}
```

Output:

```text
true
true
```

## See also

- [interface_by_name](interface_by_name.md): by its name
- [interfaces](interfaces.md): all of them

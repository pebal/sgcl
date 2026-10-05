[sgcl](../README.md) › [net](README.md)

# sgcl::net::interface_by_name

```cpp
#include "sgcl/net/interface.h"   // or "sgcl/net.h"

namespace sgcl::net {
    expected<network_interface, io::error> interface_by_name(const string& name) noexcept;
}
```

Returns the network interface of this machine named `name`: Go's `net.InterfaceByName`. The name is the system's,
`"en0"`, `"eth0"`, `"lo0"` on macOS and `"lo"` on Linux for the loopback.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the interface's name |

## Return value

The [network_interface](network_interface.md); or the [io::error](../io/error/README.md), its operation `interface`
and its path `name`: `ENXIO` for a name no interface has (the empty one among them), the `errno` of `getifaddrs`.

## Complexity

What [interfaces](interfaces.md) costs: the whole list is read.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    auto none = net::interface_by_name("no-such0");
    println("{} {}", none.error().code() == std::errc::no_such_device_or_address, none.error().path());
}
```

Output:

```text
true no-such0
```

## See also

- [interface_by_index](interface_by_index.md): by its index
- [interfaces](interfaces.md): all of them

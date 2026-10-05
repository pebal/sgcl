[sgcl](../../README.md) › [net](../README.md) › [mdns](../mdns/README.md) › [responder](README.md)

# sgcl::net::mdns::responder::host_name

```cpp
string host_name() const noexcept;
```

Returns the host's name as the responder holds it now, absolute: `"mymac.local."`, or `"mymac-2.local."` after a
conflict renamed it (RFC 6762 §9). A service published without a host of its own points to it.

## Parameters

None.

## Return value

The name, with `.local.` and its trailing dot.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::mdns::options o;
    auto all = net::interfaces();
    for (auto& i : *all) {
        if (i.loopback) {
            o.interfaces.push_back(i);
        }
    }
    o.host = "docs-host.local";  // ".local" may be written
    net::mdns::responder r = net::mdns::responder::start(o).value();
    println("{}", r.host_name());
    r.close();
}
```

Output:

```text
docs-host.local.
```

## See also

- [start](start.md): where the name is probed for
- [sgcl::net::mdns::responder](README.md)

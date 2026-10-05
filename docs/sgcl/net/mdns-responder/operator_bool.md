[sgcl](../../README.md) › [net](../README.md) › [mdns](../mdns/README.md) › [responder](README.md)

# sgcl::net::mdns::responder::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the handle holds a responder: `false` for one made by the default constructor, `true` for any made by
[start](start.md) or [dns_sd::publish](../dns_sd/publish.md), closed or not.

## Parameters

None.

## Return value

`true` when the handle holds a responder.

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
    net::mdns::responder none;
    net::mdns::options o;
    auto all = net::interfaces();
    for (auto& i : *all) {
        if (i.loopback) {
            o.interfaces.push_back(i);
        }
    }
    o.host = "docs-bool";
    net::mdns::responder r = net::mdns::responder::start(o).value();
    r.close();
    println("{} {}", static_cast<bool>(none), static_cast<bool>(r));
}
```

Output:

```text
false true
```

## See also

- [(constructor)](mdns-responder.md): a handle that holds none
- [sgcl::net::mdns::responder](README.md)

[sgcl](../../README.md) › [net](../README.md) › [mdns](../mdns/README.md) › [responder](README.md)

# sgcl::net::mdns::responder::responder

```cpp
responder() noexcept;                          // (1)
responder(const responder& other) noexcept;    // (2), implicitly declared
responder(responder&& other) noexcept;         // (3), implicitly declared
```

1. A handle that holds no responder: an operation on it is a contract violation (debug builds assert), and
   [operator bool](operator_bool.md) says `false`. A responder is made by [start](start.md) or
   [dns_sd::publish](../dns_sd/publish.md).
2. A handle of the same responder as `other`: its publish, close and every other call reach the one state.
3. The same, `other` left holding no responder.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle copied or moved |

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
    println("{}", static_cast<bool>(none));
    net::mdns::options o;
    auto all = net::interfaces();
    for (auto& i : *all) {
        if (i.loopback) {
            o.interfaces.push_back(i);
        }
    }
    o.host = "docs-copy";
    net::mdns::responder r = net::mdns::responder::start(o).value();
    net::mdns::responder copy = r;
    copy.close();  // the one responder
    net::dns_sd::service s;
    s.name = "Late";
    s.type = "_http._tcp";
    println("{}", r.publish(s).error().message());
}
```

Output:

```text
false
publish Late: stream closed
```

## See also

- [start](start.md): a responder that holds one
- [operator==](operator_cmp.md): two handles of one responder
- [sgcl::net::mdns::responder](README.md)

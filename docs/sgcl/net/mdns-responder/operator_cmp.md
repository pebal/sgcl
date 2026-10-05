[sgcl](../../README.md) › [net](../README.md) › [mdns](../mdns/README.md) › [responder](README.md)

# sgcl::net::operator==, operator!= (sgcl::net::mdns::responder)

```cpp
friend bool operator==(const responder& a, const responder& b) noexcept;
```

Checks whether `a` and `b` are handles of the same responder: copies of one handle, not two responders of one host.
Two handles that hold none are equal. `a != b` is `!(a == b)`, rewritten by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when the two hold the same responder, or both none; `false` otherwise.

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
    o.host = "docs-cmp";
    net::mdns::responder r = net::mdns::responder::start(o).value();
    net::mdns::responder copy = r;
    net::mdns::responder other = net::mdns::responder::start(o).value();
    println("{} {} {}", r == copy, r == other, r != other);
    r.close();
    other.close();
}
```

Output:

```text
true false true
```

## See also

- [(constructor)](mdns-responder.md): a copy that is the same responder
- [sgcl::net::mdns::responder](README.md)

[sgcl](../../../README.md) › [net](../../README.md) › [dns_sd](../README.md) › [browser](README.md)

# sgcl::net::dns_sd::browser::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the handle holds a browse: `false` for one made by the default constructor, `true` for any made by
[browse](../browse.md), closed or not.

## Parameters

None.

## Return value

`true` when the handle holds a browse.

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
    net::dns_sd::browser none;
    net::mdns::options o;
    auto all = net::interfaces();
    for (auto& i : *all) {
        if (i.loopback) {
            o.interfaces.push_back(i);
        }
    }
    net::dns_sd::browser b = net::dns_sd::browse("_docs-bool._tcp", o).value();
    b.close();
    println("{} {}", static_cast<bool>(none), static_cast<bool>(b));
}
```

Output:

```text
false true
```

## See also

- [(constructor)](browser.md): a handle that holds none
- [sgcl::net::dns_sd::browser](README.md)

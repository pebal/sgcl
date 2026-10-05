[sgcl](../../../README.md) › [net](../../README.md) › [dns_sd](../README.md) › [browser](README.md)

# sgcl::net::dns_sd::operator==, operator!= (sgcl::net::dns_sd::browser)

```cpp
friend bool operator==(const browser& a, const browser& b) noexcept;
```

Checks whether `a` and `b` are handles of the same browse: copies of one handle, not two browses of one type. Two
handles that hold none are equal. `a != b` is `!(a == b)`, rewritten by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when the two hold the same browse, or both none; `false` otherwise.

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
    net::dns_sd::browser b = net::dns_sd::browse("_docs-cmp._tcp", o).value();
    net::dns_sd::browser copy = b;
    net::dns_sd::browser other = net::dns_sd::browse("_docs-cmp._tcp", o).value();
    println("{} {} {}", b == copy, b == other, b != other);
    b.close();
    other.close();
}
```

Output:

```text
true false true
```

## See also

- [(constructor)](browser.md): a copy that is the same browse
- [sgcl::net::dns_sd::browser](README.md)

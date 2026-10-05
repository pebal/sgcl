[sgcl](../../../README.md) › [net](../../README.md) › [dns_sd](../README.md) › [browser](README.md)

# sgcl::net::dns_sd::browser::browser

```cpp
browser() noexcept;                        // (1)
browser(const browser& other) noexcept;    // (2), implicitly declared
browser(browser&& other) noexcept;         // (3), implicitly declared
```

1. A handle that holds no browse: an operation on it is a contract violation (debug builds assert), and
   [operator bool](operator_bool.md) says `false`. A browse is made by [browse](../browse.md).
2. A handle of the same browse as `other`: a `next` of either takes the next event, a `close` of either ends it.
3. The same, `other` left holding no browse.

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
    net::dns_sd::browser none;
    println("{}", static_cast<bool>(none));
    net::mdns::options o;
    auto all = net::interfaces();
    for (auto& i : *all) {
        if (i.loopback) {
            o.interfaces.push_back(i);
        }
    }
    net::dns_sd::browser b = net::dns_sd::browse("_docs-copy._tcp", o).value();
    net::dns_sd::browser copy = b;
    copy.close();
    println("{}", b.next().error().message());
}
```

Output:

```text
false
browse _docs-copy._tcp.local.: stream closed
```

## See also

- [browse](../browse.md): a browser that holds one
- [sgcl::net::dns_sd::browser](README.md)

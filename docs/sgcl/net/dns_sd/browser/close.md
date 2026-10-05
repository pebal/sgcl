[sgcl](../../../README.md) › [net](../../README.md) › [dns_sd](../README.md) › [browser](README.md)

# sgcl::net::dns_sd::browser::close

```cpp
void close() const noexcept;
```

Ends the browse: its questions are no longer asked, the events not taken are dropped, and a [next](next.md) waiting or
to come gives `io::errc::closed`. The engine under it is closed with the last browse and lookup of its interfaces. A
second close does nothing.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the browses of the engine.

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
    net::dns_sd::browser b = net::dns_sd::browse("_docs-close._tcp", o).value();
    b.close();
    b.close();
    println("{}", b.next().error().message());
}
```

Output:

```text
browse _docs-close._tcp.local.: stream closed
```

## See also

- [next](next.md): what it ends
- [sgcl::net::dns_sd::browser](README.md)

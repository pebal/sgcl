[sgcl](../../README.md) › [net](../README.md) › [mdns](../mdns/README.md) › [responder](README.md)

# sgcl::net::mdns::responder::services

```cpp
vector<dns_sd::service> services() const noexcept;
```

Returns the services published on the responder and not withdrawn, in the order they were published, each as it was
given with its name as the responder holds it now: a conflict after the publication may have renamed it.

## Parameters

None.

## Return value

The services, empty when none is published.

## Complexity

Linear in the services.

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
    net::dns_sd::service s;
    s.name = "Docs Services";
    s.type = "_http._tcp";
    s.port = 8000;
    net::mdns::responder r = net::dns_sd::publish(s, o).value();
    for (auto& p : r.services()) {
        println("{} {} {}", p.name, p.type, p.port);
    }
    r.close();
    println("{}", r.services().size());
}
```

Output:

```text
Docs Services _http._tcp 8000
0
```

## See also

- [publish](publish.md), [remove](remove.md): what changes it
- [sgcl::net::mdns::responder](README.md)

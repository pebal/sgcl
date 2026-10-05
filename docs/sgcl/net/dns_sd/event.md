[sgcl](../../README.md) › [net](../README.md) › [dns_sd](README.md)

# sgcl::net::dns_sd::event

```cpp
#include "sgcl/net/mdns.h"   // or "sgcl/net.h"

namespace sgcl::net::dns_sd {
    struct event {
        bool added = false;
        string name;
        string type;
        string domain;
        uint32_t interface = 0;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::dns_sd::event` is what a [browser](browser/README.md)'s `next` gives: an instance come (its PTR in the
cache) or gone (its goodbye, or its record expired), with its name, type and domain as [resolve](resolve.md) takes
them, and the interface it was seen on. Of a browse of `"_services._dns-sd._udp"` the name is a type
(`"_http._tcp"`). A plain struct of fields.

## Member objects

| Member | Description |
|---|---|
| `added` | `true` for an instance come, `false` for one gone |
| `name` | the instance's name (`"Living Room"`) |
| `type` | its type (`"_http._tcp"`); of a subtype's browse, the type, not the subtype |
| `domain` | its domain, absolute (`"local."`) |
| `interface` | the index of the interface its PTR first came on |

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
    s.name = "Docs Event";
    s.type = "_docs-event._tcp";
    s.port = 1234;
    net::mdns::responder r = net::dns_sd::publish(s, o).value();
    net::dns_sd::browser b = net::dns_sd::browse("_docs-event._tcp", o).value();
    auto come = b.next().value();
    println("{} {} {} {}", come.added, come.name, come.type, come.domain);
    r.close();  // its goodbye
    auto gone = b.next().value();
    println("{} {}", gone.added, gone.name);
    b.close();
}
```

Output:

```text
true Docs Event _docs-event._tcp local.
false Docs Event
```

## See also

- [browser::next](browser/next.md): what gives it
- [resolve](resolve.md): its host and port
- [sgcl::net::dns_sd](README.md)

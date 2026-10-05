[sgcl](../../README.md) › [net](../README.md) › [mdns](../mdns/README.md) › [responder](README.md)

# sgcl::net::mdns::responder::remove, async_remove

```cpp
bool remove(const string& name, const string& type) const;                                // (1)
async::task<bool> async_remove(const string& name, const string& type) const noexcept;    // (2)
```

Withdraws a service published on the responder: its records said goodbye to (sent with TTL 0, RFC 6762 §10.1), so
that the browsers of the link see it go at once, and no longer answered. The service is named by its name as
[publish](publish.md) returned it and its type.

1. Waits on the calling thread for the goodbye to be sent: for a thread, as [task::wait](../../async/task/wait.md)
   is (debug builds assert).
2. The same for a task.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the instance's name as held (`"Printer (2)"` after a rename) |
| `type` | its type, `"_ipp._tcp"` |

## Return value

`true` when the service was published and is withdrawn, `false` when none of that name and type was.

## Complexity

Linear in the services published; a goodbye on each interface and family.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

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
    net::mdns::responder r = net::mdns::responder::start(o).value();
    net::dns_sd::service s;
    s.name = "Docs Remove";
    s.type = "_http._tcp";
    s.port = 80;
    r.publish(s).value();
    net::dns_sd::browser b = net::dns_sd::browse("_http._tcp", o).value();
    for (;;) {
        auto e = b.next().value();
        if (e.name == "Docs Remove") {
            println("{} {}", e.added ? "added" : "removed", e.name);
            if (!e.added) {
                break;
            }
            println("{}", r.remove("Docs Remove", "_http._tcp"));
            println("{}", r.remove("Docs Remove", "_http._tcp"));
        }
    }
    b.close();
    r.close();
}
```

Output:

```text
added Docs Remove
true
false
removed Docs Remove
```

## See also

- [publish](publish.md): the other way
- [close](close.md): every service and the host withdrawn
- [sgcl::net::mdns::responder](README.md)

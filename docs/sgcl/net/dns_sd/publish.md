[sgcl](../../README.md) › [net](../README.md) › [dns_sd](README.md)

# sgcl::net::dns_sd::publish, async_publish

```cpp
expected<mdns::responder, io::error> publish(const service& s, const mdns::options& o = {});              // (1)
async::task<expected<mdns::responder, io::error>> async_publish(const service& s,                         // (2)
                                                                const mdns::options& o = {}) noexcept;
expected<mdns::responder, io::error> publish(const string& name, const string& type, uint16_t port,       // (3)
                                             const txt_record& txt = {});
```

Publishes a service by a responder of its own: [responder::start](../mdns-responder/start.md) with `o` (the host's
name probed for and announced) and [responder::publish](../mdns-responder/publish.md) of `s` (the instance's name
probed for, renamed `"Name (2)"` when another host holds it, its PTR, SRV, TXT and subtypes announced). Returns the
responder, which answers for the service until it is [closed](../mdns-responder/close.md); its
[services](../mdns-responder/services.md) says the name held. About two seconds of probing, the host's and the
service's.

1. Waits on the calling thread: for a thread, as [task::wait](../../async/task/wait.md) is (debug builds assert).
2. The same for a task.
3. The one-line form of (1): a service of the name, type, port and TXT record, on every interface.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the service: its name, type, port, TXT record, subtypes; its host the responder's unless it names one ([service](service.md)) |
| `o` | the interfaces, families and host name of the responder ([mdns::options](../mdns-options.md)) |
| `name`, `type`, `port`, `txt` | (3) the service's name, type, port and TXT record |

## Return value

The responder. Or the [io::error](../../io/error/README.md) of the start or of the publication (`EINVAL` for a service
that is none), the responder then closed.

## Complexity

The probes and announcements of the host and of the service on each interface and family.

## Exceptions

- (1, 3) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
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
    o.host = "docs-publish";
    net::dns_sd::service s;
    s.name = "Docs Publish";
    s.type = "_http._tcp";
    s.port = 8080;
    s.txt = {{"path", "/"}};
    net::mdns::responder r = net::dns_sd::publish(s, o).value();
    println("{} on {}", r.services()[0].name, r.host_name());
    r.close();
}
```

Output:

```text
Docs Publish on docs-publish.local.
```

## See also

- [mdns::responder](../mdns-responder/README.md): several services on one responder
- [browse](browse.md), [resolve](resolve.md): the other side
- [sgcl::net::dns_sd](README.md)

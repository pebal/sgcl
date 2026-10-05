[sgcl](../../README.md) › [net](../README.md) › [mdns](../mdns/README.md) › [responder](README.md)

# sgcl::net::mdns::responder::publish, async_publish

```cpp
expected<string, io::error> publish(const dns_sd::service& s) const;                                // (1)
async::task<expected<string, io::error>> async_publish(const dns_sd::service& s) const noexcept;    // (2)
```

Publishes a service on the responder (RFC 6763): the PTR of its type (`_http._tcp.local.`) to the instance, the
instance's SRV (the host, the port) and TXT, the type's PTR under `_services._dns-sd._udp.local.` (§9), a PTR per
subtype under `_sub` (§7.1); the instance's name probed for and the records announced. Returns the name held, the
service's own or, when another host holds it, the next, `"Printer (2)"` (RFC 6762 §9).

The service's host is the responder's ([host_name](host_name.md)) unless it names one; its domain is `local.` unless
it names one. Its addresses and interface are not read.

1. Waits on the calling thread: for a thread, as [task::wait](../../async/task/wait.md) is (debug builds assert).
2. The same for a task.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the service: its name, type, port, TXT record, subtypes, and optionally its host and domain ([dns_sd::service](../dns_sd/service.md)) |

## Return value

The instance's name as held. Or the [io::error](../../io/error/README.md), its operation `publish`: `EINVAL` for a
name empty or past 63 bytes, a type that is not `_name._tcp` or `_name._udp` (the name 1 to 15 letters, digits and
`-`), a subtype without its underscore, a host that is no name; `io::errc::closed` once the responder was closed.

## Complexity

Three probes and two announcements on each interface and family.

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
    net::mdns::responder first = net::mdns::responder::start(o).value();
    net::mdns::responder second = net::mdns::responder::start(o).value();
    net::dns_sd::service printer;
    printer.name = "Docs Printer";
    printer.type = "_ipp._tcp";
    printer.port = 631;
    printer.subtypes = {"_universal"};
    println("{}", first.publish(printer).value());
    printer.port = 632;  // the same name, another service: a conflict
    println("{}", second.publish(printer).value());
    printer.type = "ipp";
    println("{}", first.publish(printer).error().message());
    first.close();
    second.close();
}
```

Output:

```text
Docs Printer
Docs Printer (2)
publish Docs Printer.ipp: Invalid argument
```

## See also

- [remove](remove.md): the service withdrawn
- [services](services.md): what is published
- [dns_sd::browse](../dns_sd/browse.md), [dns_sd::resolve](../dns_sd/resolve.md): the other side
- [sgcl::net::mdns::responder](README.md)

[sgcl](../../README.md) › [net](../README.md) › [dns_sd](README.md)

# sgcl::net::dns_sd::resolve, async_resolve

```cpp
expected<service, io::error> resolve(const string& name, const string& type, const mdns::options& o = {});    // (1)
async::task<expected<service, io::error>> async_resolve(const string& name, const string& type,               // (2)
                                                        const mdns::options& o = {},
                                                        async::stop_token stop = {}) noexcept;
```

Resolves a service instance (RFC 6763 §4.2): its SRV and TXT asked of the link together, then the A and AAAA of the
SRV's target unless they came with them in the additional section. Returns a [service](service.md) with the host,
the port, the TXT record (its entries kept by §6.4), the host's addresses (IPv4's first, an IPv6 link-local one with
its interface's zone) and the interface it answered on. Records the cache holds answer at once.

1. Waits on the calling thread: for a thread, as [task::wait](../../async/task/wait.md) is (debug builds assert).
2. The same for a task, which holds no worker while it waits; a stop ends the wait with `ECANCELED`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the instance's name, as an [event](event.md) gives it (`"Living Room"`) |
| `type` | its type, `"_http._tcp"` (or with its domain, `"_http._tcp.local."`) |
| `o` | the interfaces, families, wait (2 s by default) and QU ([mdns::options](../mdns-options.md)) |
| `stop` | ends the wait when stopped; none by default |

## Return value

The service. Or the [io::error](../../io/error/README.md), its operation `resolve` and its path the name and type:
`net::errc::host_not_found` when no SRV came within the wait, `EINVAL` for a name empty or past 63 bytes or a type
that is none, `ECANCELED` for the stop (2). An instance whose host did not answer for its addresses within half a
second more has none.

## Complexity

Two one-shot queries at most, each sent at most three times.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    net::mdns::options o;
    auto all = net::interfaces();
    for (auto& i : *all) {
        if (i.loopback) {
            o.interfaces.push_back(i);
        }
    }
    o.host = "docs-resolve";
    net::dns_sd::service s;
    s.name = "Docs Resolve";
    s.type = "_http._tcp";
    s.port = 8443;
    s.txt = {{"path", "/admin"}, {"tls", "1"}};
    net::mdns::responder r = net::dns_sd::publish(s, o).value();
    auto found = net::dns_sd::resolve("Docs Resolve", "_http._tcp", o).value();
    println("{} {} {}", found.host, found.port, found.addresses.front());
    for (auto& e : found.txt.entries()) {
        println("{}", e);
    }
    o.timeout = 200ms;
    println("{}", net::dns_sd::resolve("Nobody", "_http._tcp", o).error().message());
    r.close();
}
```

Output:

```text
docs-resolve.local. 8443 127.0.0.1
path=/admin
tls=1
resolve Nobody._http._tcp: no such host
```

## See also

- [browse](browse.md): the instances to resolve
- [service](service.md): what it gives
- [sgcl::net::dns_sd](README.md)

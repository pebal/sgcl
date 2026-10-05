[sgcl](../../README.md) › [net](../README.md) › [dns_sd](README.md)

# sgcl::net::dns_sd::types, async_types

```cpp
expected<vector<string>, io::error> types(const mdns::options& o = {});                                // (1)
async::task<expected<vector<string>, io::error>> async_types(const mdns::options& o = {},              // (2)
                                                             async::stop_token stop = {}) noexcept;
```

Returns the service types of the link (RFC 6763 §9): the PTRs of `_services._dns-sd._udp.local.` that answered
within the wait, each type once (`"_http._tcp"`, without its domain), sorted. The query is sent at once and again a
second and three seconds later within the wait; every answer counts, so the call takes the whole wait.

1. Waits on the calling thread: for a thread, as [task::wait](../../async/task/wait.md) is (debug builds assert).
2. The same for a task; a stop ends the wait with `ECANCELED`.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the interfaces, families and wait (2 s by default) ([mdns::options](../mdns-options.md)) |
| `stop` | ends the wait when stopped; none by default |

## Return value

The types, empty when none answered. Or the [io::error](../../io/error/README.md), its operation `types`:
`ECANCELED` for the stop (2), the error of the sockets when no interface could be joined.

## Complexity

The wait; a query on each interface and family, sent at most three times.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

#include <algorithm>

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
    net::dns_sd::service s;
    s.name = "Docs Types";
    s.type = "_docs-types._tcp";
    s.port = 7000;
    net::mdns::responder r = net::dns_sd::publish(s, o).value();
    o.timeout = 1500ms;
    auto found = net::dns_sd::types(o).value();
    println("{}", std::ranges::find(found, string("_docs-types._tcp")) != found.end());
    r.close();
}
```

Output:

```text
true
```

## See also

- [browse](browse.md): `"_services._dns-sd._udp"` as a stream
- [sgcl::net::dns_sd](README.md)

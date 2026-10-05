[sgcl](../../../README.md) › [net](../../README.md) › [dns_sd](../README.md) › [browser](README.md)

# sgcl::net::dns_sd::browser::next, async_next

```cpp
expected<event, io::error> next() const;                                                           // (1)
async::task<expected<event, io::error>> async_next(async::stop_token stop = {}) const noexcept;    // (2)
```

Returns the next instance come or gone, an [event](../event.md), waiting for one when none is there: those that came
while the program was busy wait in the browser, in their order. An instance comes when its PTR enters the program's
cache and goes when its goodbye comes (a second later, RFC 6762 §10.1) or its record expires.

1. Waits on the calling thread: for a thread, as [task::wait](../../../async/task/wait.md) is (debug builds assert).
2. The same for a task, which holds no worker while it waits; a stop of `stop` ends the wait with `ECANCELED`.

## Parameters

| Parameter | Description |
|---|---|
| `stop` | ends the wait when stopped; none by default |

## Return value

The event. Or the [io::error](../../../io/error/README.md), its operation `browse` and its path the name browsed:
`io::errc::closed` once the browser is closed, `ECANCELED` for the stop (2).

## Complexity

Constant for an event waiting; else the wait.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/async.h"
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
    net::dns_sd::service s;
    s.name = "Docs Next";
    s.type = "_docs-next._tcp";
    s.port = 2;
    net::mdns::responder r = net::dns_sd::publish(s, o).value();
    net::dns_sd::browser b = net::dns_sd::browse("_docs-next._tcp", o).value();
    auto e = b.next().value();
    println("{} {}", e.added, e.name);
    async::stop_source stop;
    stop.stop_after(200ms);  // nothing more comes
    println("{}", b.async_next(stop.token()).wait().error().message());
    b.close();
    r.close();
}
```

Output:

```text
true Docs Next
browse _docs-next._tcp.local.: Operation canceled
```

## See also

- [event](../event.md): what it gives
- [close](close.md): the end of the browse
- [sgcl::net::dns_sd::browser](README.md)

[sgcl](../../README.md) › [net](../README.md) › [mdns](../mdns/README.md) › [responder](README.md)

# sgcl::net::mdns::responder::start, async_start

```cpp
static expected<responder, io::error> start();                                                // (1)
static expected<responder, io::error> start(const options& o);                                // (2)
static async::task<expected<responder, io::error>> async_start() noexcept;                    // (3)
static async::task<expected<responder, io::error>> async_start(const options& o) noexcept;    // (4)
```

Starts a responder: its sockets on the interfaces, port 5353 shared with the system's responder, the host's name
probed for and announced with the addresses of each interface (A and AAAA, TTL two minutes). Returns once the name
is held, about a second after the call; a name another host holds is replaced by the next, `"mymac-2"`, and
[host_name](host_name.md) says which.

- (1, 3) Every interface up and able to multicast, this machine's name; (2, 4) the interfaces, families and host of
  `o` ([mdns::options](../mdns-options.md)).
- (1–2) Run on the scheduler, waited for on the calling thread: for a thread, as
  [task::wait](../../async/task/wait.md) is (debug builds assert); a task awaits (3–4).

## Parameters

| Parameter | Description |
|---|---|
| `o` | the interfaces, families and host name |

## Return value

The responder. Or the [io::error](../../io/error/README.md), its operation `mdns`: `EINVAL` for a host name that is
not one label of at most 63 bytes (`"a.b"`), the error of the sockets when no interface could be joined
(`EADDRNOTAVAIL` when there are none).

## Complexity

Three probes and two announcements on each interface and family.

## Exceptions

- (1–2) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (3–4) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

async::task<> two(net::mdns::options o) {
    auto first = co_await net::mdns::responder::async_start(o);
    auto second = co_await net::mdns::responder::async_start(o);  // the same name, the same addresses: no conflict
    println("{} {}", first->host_name(), second->host_name());
    co_await first->async_close();
    co_await second->async_close();
}

int main() {
    net::mdns::options o;
    auto all = net::interfaces();
    for (auto& i : *all) {
        if (i.loopback) {
            o.interfaces.push_back(i);
        }
    }
    o.host = "docs-start";
    async::run(two(o));
    o.host = "a.b";
    println("{}", net::mdns::responder::start(o).error().message());
}
```

Output:

```text
docs-start.local. docs-start.local.
mdns a.b: Invalid argument
```

## See also

- [publish](publish.md): a service on it
- [dns_sd::publish](../dns_sd/publish.md): a responder for one service
- [close](close.md): the end
- [sgcl::net::mdns::responder](README.md)

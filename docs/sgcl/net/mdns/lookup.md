[sgcl](../../README.md) › [net](../README.md) › [mdns](README.md)

# sgcl::net::mdns::lookup, async_lookup

```cpp
static expected<vector<ip_address>, io::error> lookup(const string& host);                                         // (1)
static expected<vector<ip_address>, io::error> lookup(const string& host, const options& o);                       // (2)
static async::task<expected<vector<ip_address>, io::error>> async_lookup(const string& host,                       // (3)
                                                                         async::stop_token stop = {}) noexcept;
static async::task<expected<vector<ip_address>, io::error>> async_lookup(const string& host, const options& o,     // (4)
                                                                         async::stop_token stop = {}) noexcept;
```

Returns the addresses of a host of the link by multicast DNS (RFC 6762): its A and AAAA records asked of the link
together, the answers of the first host that answers. `.local` is added to a name without it (`"printer"` is
`"printer.local."`). The IPv4 addresses come first; an IPv6 link-local address comes with the zone of the interface
it was seen on (`fe80::1%en0`), so that it can be connected to as it is.

- (1, 3) Every interface up and able to multicast; (2, 4) those of `o` ([mdns::options](../mdns-options.md)), its
  families, its wait, a first question asking for a unicast answer (QU, §5.4).
- (1–2) Run the lookup on the scheduler and wait for it on the calling thread: for a thread, as
  [task::wait](../../async/task/wait.md) is (debug builds assert); a task awaits (3–4).
- (3–4) The same for a task, which holds no worker while it waits. A stop of `stop` ends the wait with `ECANCELED`.

Records the cache already holds answer at once; else the question is sent, and again one and three seconds later,
until an answer comes or the timeout passes.

## Parameters

| Parameter | Description |
|---|---|
| `host` | the host's name, with or without `.local` and the trailing dot |
| `o` | the interfaces, families, wait and QU of the lookup |
| `stop` | ends the wait when stopped; none by default |

## Return value

The addresses, at least one, each once. Or the [io::error](../../io/error/README.md), its operation `lookup` and its
path `host`: `net::errc::host_not_found` when no host answered within the timeout, and for an empty name or one that
is no name; `ECANCELED` for the stop (3–4); the error of the sockets when none of the interfaces could be joined
(`EADDRNOTAVAIL` when there are none).

## Complexity

A query on each interface and family, sent at most three times.

## Exceptions

- (1–2) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (3–4) None.

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
    o.host = "docs-lookup";
    net::mdns::responder r = net::mdns::responder::start(o).value();
    auto found = net::mdns::lookup("docs-lookup", o);
    for (auto& a : *found) {
        println("{}", a);
    }
    o.timeout = 200ms;
    println("{}", net::mdns::lookup("nobody-here", o).error().message());
    r.close();
}
```

Sample output:

```text
127.0.0.1
::1
fe80::1%lo0
lookup nobody-here: no such host
```

## See also

- [responder](../mdns-responder/README.md): the side that answers
- [dns::lookup](../dns/lookup.md): a name under `.local` among the others
- [sgcl::net::mdns](README.md)

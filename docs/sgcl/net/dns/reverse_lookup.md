[sgcl](../../README.md) › [net](../README.md) › [dns](README.md)

# sgcl::net::dns::reverse_lookup, async_reverse_lookup

```cpp
static expected<vector<string>, io::error> reverse_lookup(const ip_address& address) noexcept;                         // (1)
static async::task<expected<vector<string>, io::error>> async_reverse_lookup(const ip_address& address,                // (2)
                                                                             async::stop_token stop = {}) noexcept;
static expected<vector<string>, io::error> reverse_lookup(const ip_address& address, const options& o);                // (3)
static async::task<expected<vector<string>, io::error>> async_reverse_lookup(const ip_address& address,                // (4)
                                                                             const options& o,
                                                                             async::stop_token stop = {}) noexcept;
```

Returns the names of `address`.

- (1–2) Through the system's resolver (`getnameinfo`): Go's `net.LookupAddr`. The system gives one name, its own for
  the address (`/etc/hosts`, the system's cache, a query), without a trailing dot.
- (3–4) Through the module's own resolver with the servers of `o` ([dns::options](../dns-options.md)): the PTR
  records of the address's name under `in-addr.arpa.` or `ip6.arpa.` (RFC 1035 §3.5, RFC 3596 §2.5), each name
  absolute, with its trailing dot, in the answer's order; a link-local address's (`169.254.0.0/16`, `fe80::/10`) by
  multicast DNS (RFC 6762 §4).

1. On the calling thread, which the resolver blocks. It takes no stop, as [lookup](lookup.md).
2. The same for a task: the call runs on the [blocking pool](../../async/spawn_blocking.md), from the task's first
   run on (nothing starts at the call), and the task holds no worker while it waits. A stop of `stop` ends the wait
   with `ECANCELED`; a job given up before a thread took it does not call the resolver.
3. Runs the lookup on the scheduler and waits for it on the calling thread, as [lookup_mx](lookup_mx.md) does.
4. The same for a task; a stop ends the wait with `ECANCELED` at once.

## Parameters

| Parameter | Description |
|---|---|
| `address` | the address to name |
| `o` | the servers, the wait for one answer, the rounds ([dns::options](../dns-options.md)) |
| `stop` | ends the wait when stopped; none by default |

## Return value

The names: (1–2) one, (3–4) at least one. Or the [io::error](../../io/error/README.md), its operation `lookup`:

- (1–2) its path the address: `net::errc::invalid_address` for the empty address and for a zone that names no
  interface, neither asked of the resolver; `net::errc::host_not_found` for an address with no name, an `EAI_*`
  code in [lookup_category](../lookup_category.md) for a failure of the resolver, `ECANCELED` for the stop (2);
- (3–4) its path the PTR's name (`"1.0.0.127.in-addr.arpa."`): the errors of [lookup_mx](lookup_mx.md),
  `net::errc::no_data` for an address whose name has no PTR.

## Complexity

- (1–2) What the resolver takes.
- (3–4) One query per server and attempt until one answers; no cache.

## Exceptions

- (1) None.
- (2) None. A thread of the blocking pool that cannot be started is the task's: its `co_await` rethrows the
  `std::system_error`.
- (3) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (4) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

async::task<> stopped() {
    async::stop_source source;
    source.request_stop();
    net::ip_address loopback = net::ip_address::loopback_v4();
    auto names = co_await net::dns::async_reverse_lookup(loopback, source.token());
    println("{}", names.error().message());
}

int main() {
    auto names = net::dns::reverse_lookup(net::ip_address::loopback_v4());
    println("{}", names->size());
    async::run(stopped());
}
```

Output:

```text
1
lookup 127.0.0.1: Operation canceled
```

Through the module's own resolver, the names of a PTR:

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    net::udp::socket silent = net::udp::bind("127.0.0.1:0");  // a server that never answers
    net::dns::options o;
    o.servers = {silent.local_endpoint().to_string()};
    o.timeout = 100ms;
    o.attempts = 1;
    auto names = net::dns::reverse_lookup(net::ip_address("192.0.2.1"), o);
    println("{}", names.error().message());
    auto six = net::dns::reverse_lookup(net::ip_address("2001:db8::1"), o);
    println("{}", six.error().message());
}
```

Output:

```text
lookup 1.2.0.192.in-addr.arpa.: Operation timed out
lookup 1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.8.b.d.0.1.0.0.2.ip6.arpa.: Operation timed out
```

## See also

- [lookup, async_lookup](lookup.md): the other way
- [dns::options](../dns-options.md): the servers of (3–4)
- [sgcl::net::dns](README.md)

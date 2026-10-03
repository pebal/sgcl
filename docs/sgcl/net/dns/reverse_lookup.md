[sgcl](../../README.md) › [net](../README.md) › [dns](../dns.md)

# sgcl::net::dns::reverse_lookup, async_reverse_lookup

```cpp
/*(1)*/ static expected<vector<string>, io::error> reverse_lookup(const ip_address& address) noexcept;
/*(2)*/ static async::task<expected<vector<string>, io::error>> async_reverse_lookup(const ip_address& address,
                                                                                     async::stop_token stop = {}) noexcept;
```

Returns the names of `address`, through the system's resolver (`getnameinfo`): Go's `net.LookupAddr`. The system
gives one name, its own for the address (`/etc/hosts`, the system's cache, a query).

1. On the calling thread, which the resolver blocks. It takes no stop, as [lookup](lookup.md).
2. The same for a task: the call runs on the [blocking pool](../../async/spawn_blocking.md), from the task's first
   run on (nothing starts at the call), and the task holds no worker while it waits. A stop of `stop` ends the wait with `ECANCELED`; a job given up before a thread took it
   does not call the resolver.

## Parameters

| Parameter | Description |
|---|---|
| `address` | the address to name |
| `stop` | ends the wait when stopped; none by default |

## Return value

The names, one. Or the [io::error](../../io/error.md), its operation `lookup` and its path the address:
`net::errc::invalid_address` for the empty address and for a zone that names no interface, neither asked of the
resolver; `net::errc::host_not_found` for an address with no name, an `EAI_*` code in
[lookup_category](../lookup_category.md) for a failure of the resolver, `ECANCELED` for the stop (2).

## Complexity

What the resolver takes.

## Exceptions

- (1) None.
- (2) None. A thread of the blocking pool that cannot be started is the task's: its `co_await` rethrows the
  `std::system_error`.

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

## See also

- [lookup, async_lookup](lookup.md): the other way
- [sgcl::net::dns](../dns.md)

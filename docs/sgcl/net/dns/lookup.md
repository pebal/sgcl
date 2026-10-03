[sgcl](../../README.md) › [net](../README.md) › [dns](../dns.md)

# sgcl::net::dns::lookup, async_lookup

```cpp
static expected<vector<ip_address>, io::error> lookup(const string& host) noexcept;                                // (1)
static async::task<expected<vector<ip_address>, io::error>> async_lookup(const string& host,                       // (2)
                                                                         async::stop_token stop = {}) noexcept;
```

Returns the addresses of `host`, through the system's resolver (`getaddrinfo`): Go's `net.LookupIP`. The order is
the resolver's (RFC 6724 where the system sorts), each address once. A number (`"10.0.0.1"`, `"fe80::1%en0"`) is
answered at once, with no pool and no resolver, even after a stop. A name outside ASCII is converted by the caller
first, with [txt::idna](../../txt/idna.md)`::to_ascii`: lookup does not guess the profile.

1. On the calling thread, which the resolver blocks. It takes no stop: the system's resolver cannot be
   interrupted, so the call goes on to its end.
2. The same for a task: the call runs on the [blocking pool](../../async/spawn_blocking.md), bounded by
   `config::blocking_threads`, and the task holds no worker while it waits. A stop of `stop` ends the wait with
   `ECANCELED`, and wins over a result that comes at the same moment. A job given up while it waited for a thread
   of the pool does not call the resolver at all; one already inside `getaddrinfo` finishes on its own, its result
   dropped.

## Parameters

| Parameter | Description |
|---|---|
| `host` | a name, or an address as text |
| `stop` | ends the wait when stopped; none by default |

## Return value

The addresses, at least one. Or the [io::error](../../io/error.md), its operation `lookup` and its path `host`:

- `net::errc::host_not_found` (`EAI_NONAME`) for a name the resolver does not know, an empty name, a name with a NUL
  byte;
- an `EAI_*` code in [lookup_category](../lookup_category.md) for a failure of the resolver, `EAI_SYSTEM` as the
  `errno` it stands for;
- `ECANCELED` for the stop (2).

## Complexity

What the resolver takes: nothing for a number, the system's cache or a query for a name.

## Exceptions

- (1) None.
- (2) None. A thread of the blocking pool that cannot be started is the task's: its `co_await` rethrows the
  `std::system_error`.

## Notes

No cache of its own, as in Go: the system caches. A deadline of a caller, a [tcp::connect](../tcp/connect.md) with
a timeout, ends the wait of its lookup the same way with `ETIMEDOUT`.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

async::task<> stopped_lookups() {
    async::stop_source source;
    source.request_stop();
    auto name = co_await net::dns::async_lookup("localhost", source.token());
    println("{}", name.error().message());
    auto number = co_await net::dns::async_lookup("10.0.0.1", source.token());
    println("{}", number->front());  // a number needs no resolver
}

int main() {
    for (const char* host : {"10.0.0.1", "fe80::1%en0", "2001:DB8::1"}) {
        println("{}", net::dns::lookup(host)->front());
    }
    println("{}", net::dns::lookup("").error().message());
    async::run(stopped_lookups());
}
```

Output:

```text
10.0.0.1
fe80::1%en0
2001:db8::1
lookup: no such host
lookup localhost: Operation canceled
10.0.0.1
```

## See also

- [reverse_lookup](reverse_lookup.md): the other way
- [tcp::connect](../tcp/connect.md): a lookup and a connect
- [ip_address](../ip_address.md): what it gives
- [sgcl::net::dns](../dns.md)

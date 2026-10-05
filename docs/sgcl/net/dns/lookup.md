[sgcl](../../README.md) › [net](../README.md) › [dns](README.md)

# sgcl::net::dns::lookup, async_lookup

```cpp
static expected<vector<ip_address>, io::error> lookup(const string& host) noexcept;                                // (1)
static async::task<expected<vector<ip_address>, io::error>> async_lookup(const string& host,                       // (2)
                                                                         async::stop_token stop = {}) noexcept;
static expected<vector<ip_address>, io::error> lookup(const string& host, const options& o);                       // (3)
static async::task<expected<vector<ip_address>, io::error>> async_lookup(const string& host, const options& o,     // (4)
                                                                         async::stop_token stop = {}) noexcept;
```

Returns the addresses of `host`. A number (`"10.0.0.1"`, `"fe80::1%en0"`) is answered at once, with no pool and no
resolver, even after a stop. A name outside ASCII is converted by the caller first, with
[txt::idna](../../txt/idna/README.md)`::to_ascii`: lookup does not guess the profile.

- (1–2) Through the system's resolver (`getaddrinfo`): Go's `net.LookupIP`. The order is the resolver's (RFC 6724
  where the system sorts), each address once. A name under `.local` the system's resolver does not know (Linux
  without nss-mdns) is asked again by the module's multicast DNS; on macOS mDNSResponder answers those itself.
- (3–4) Through the module's own resolver, as the records are, with the servers of `o`
  ([dns::options](../dns-options.md)): over UDP, TCP, TLS or HTTPS, the A and AAAA records asked at once, the IPv4
  addresses first, then the IPv6 ones, each in the answer's order; a name under `.local` by multicast DNS (RFC
  6762, [mdns](../mdns/README.md)). Go's `net.Resolver` with `PreferGo` and a `Dial` of its own.

1. On the calling thread, which the resolver blocks. It takes no stop: the system's resolver cannot be
   interrupted, so the call goes on to its end.
2. The same for a task: the call runs on the [blocking pool](../../async/spawn_blocking.md), bounded by
   `config::blocking_threads`, and the task holds no worker while it waits. A stop of `stop` ends the wait with
   `ECANCELED`, and wins over a result that comes at the same moment. A job given up while it waited for a thread
   of the pool does not call the resolver at all; one already inside `getaddrinfo` finishes on its own, its result
   dropped.
3. Runs the lookup on the scheduler and waits for it on the calling thread: for a thread, as
   [task::wait](../../async/task/wait.md) is (debug builds assert).
4. The same for a task, which holds no worker while it waits; a stop ends the wait with `ECANCELED` at once and
   closes the socket in use.

## Parameters

| Parameter | Description |
|---|---|
| `host` | a name, or an address as text |
| `o` | the servers, the wait for one answer, the rounds, the profile of TLS ([dns::options](../dns-options.md)) |
| `stop` | ends the wait when stopped; none by default |

## Return value

The addresses, at least one. Or the [io::error](../../io/error/README.md), its operation `lookup` and its path `host`:

- `net::errc::host_not_found` (`EAI_NONAME`) for a name the resolver does not know, an empty name, a name with a NUL
  byte; (3–4) for a name with neither A nor AAAA records too;
- (1–2) an `EAI_*` code in [lookup_category](../lookup_category.md) for a failure of the resolver, `EAI_SYSTEM` as
  the `errno` it stands for;
- (3–4) the errors of [lookup_mx](lookup_mx.md): `net::errc::server_failure`, `net::errc::server_misbehaving`,
  `net::errc::invalid_address` for a server of `o` that is none, `ETIMEDOUT`, a socket's or TLS's error;
- `ECANCELED` for the stop (2, 4).

## Complexity

- (1–2) What the resolver takes: nothing for a number, the system's cache or a query for a name.
- (3–4) Two queries at once per server and attempt until one answers; no cache.

## Exceptions

- (1) None.
- (2) None. A thread of the blocking pool that cannot be started is the task's: its `co_await` rethrows the
  `std::system_error`.
- (3) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (4) None.

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

Through the module's own resolver, a server of the program's choice:

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
    println("{}", net::dns::lookup("10.0.0.1", o)->front());  // a number asks nobody
    println("{}", net::dns::lookup("example.com", o).error().message());
    o.servers = {"tls://"};
    println("{}", net::dns::lookup("example.com", o).error().message());
}
```

Output:

```text
10.0.0.1
lookup example.com: Operation timed out
lookup tls://: invalid address
```

## See also

- [reverse_lookup](reverse_lookup.md): the other way
- [dns::options](../dns-options.md): the servers of (3–4)
- [mdns::lookup](../mdns/lookup.md): a host of the link by multicast DNS alone
- [tcp::connect](../tcp/connect.md): a lookup and a connect
- [ip_address](../ip_address/README.md): what it gives
- [sgcl::net::dns](README.md)

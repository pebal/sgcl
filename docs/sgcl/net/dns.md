[sgcl](../README.md) › [net](README.md)

# sgcl::net::dns

```cpp
#include "sgcl/net/dns.h"   // or "sgcl/net.h"

namespace sgcl::net {
    struct dns;
}
```

`sgcl::net::dns` is names to addresses and back, through the system's resolver: [lookup](dns/lookup.md) is
`getaddrinfo` and Go's `net.LookupHost` and `LookupIP`, [reverse_lookup](dns/reverse_lookup.md) is `getnameinfo`
and Go's `LookupAddr`. So `/etc/hosts`, the search domains, mDNS, the resolvers a VPN or a configuration profile
installs, and the system's cache all apply. On macOS nothing else gives the right answer: the configuration is not
in `/etc/resolv.conf`, and Go calls the system's resolver there too. A structure of static functions, as
[tcp](tcp.md) is; [tcp::connect](tcp/connect.md) looks a name up through it.

The call blocks inside the C library, so the `async_` forms run it on the
[blocking pool](../async/spawn_blocking.md), bounded by [config](../core/config.md)`::blocking_threads`, and a stop
token ends their wait.

## Rules

- A number is answered at once, with no pool and no resolver: `lookup("10.0.0.1")`, `lookup("fe80::1%en0")`.
- The order is the resolver's (RFC 6724 where the system sorts), each address once.
- A name the resolver does not know is `net::errc::host_not_found` (`EAI_NONAME`), and so is an empty name; a
  failure of the resolver is its `EAI_*` code in [lookup_category](lookup_category.md), whose `message()` is
  `gai_strerror`'s.
- A stop ends the wait, never the call: a job given up while it waited for a thread of the pool does not call the
  resolver at all, and one already inside `getaddrinfo` finishes on its own, its result dropped. A burst of lookups
  given up holds at most the pool's threads, for as long as the resolver takes (a name under `.local` that nobody
  answers: five seconds on macOS).
- A name outside ASCII is converted by the caller first, with [txt::idna](../txt/idna.md)`::to_ascii`: lookup does
  not guess the profile.
- No cache of its own, as in Go: the system caches. MX, SRV, TXT and a resolver of the module's own (RFC 1035) come
  with the Linux port.

## Member functions

| Function | Description |
|---|---|
| [lookup, async_lookup](dns/lookup.md) | the addresses of a name (static) |
| [reverse_lookup, async_reverse_lookup](dns/reverse_lookup.md) | the name of an address (static) |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

#include <algorithm>

using namespace sgcl;
using namespace std::chrono_literals;

async::task<> show(string host) {
    async::stop_source source;
    source.stop_after(2s);  // a lookup of two seconds at most
    auto found = co_await net::dns::async_lookup(host, source.token());
    if (!found) {
        println("{}", found.error().message());
        co_return;
    }
    bool loopback = std::ranges::all_of(*found, [](auto& a) { return a.is_loopback(); });
    println("{}: all loopback {}", host, loopback);
}

int main() {
    async::run(show("localhost"));
    async::run(show(""));
}
```

Output:

```text
localhost: all loopback true
lookup: no such host
```

## See also

- [tcp::connect](tcp/connect.md): looks the name up with the same stop and limit
- [ip_address](ip_address.md): what a lookup gives
- [spawn_blocking](../async/spawn_blocking.md), [stop_token](../async/stop_token.md)
- `tests/net/dial.cpp` (`NetDns_Tests`): `localhost`, numbers without the pool, RFC 6761 `.invalid`, a wait ended by
  its token and by its deadline

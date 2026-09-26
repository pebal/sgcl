# sgcl::net::dns

```cpp
#include "sgcl/net/dns.h"   // or "sgcl/net/net.h"

namespace sgcl::net {
    struct dns {
        static expected<vector<ip_address>, io::error> lookup(const string& host);
        static async::task<expected<vector<ip_address>, io::error>> async_lookup(const string& host, async::stop_token stop = {});
        static expected<vector<string>, io::error> reverse_lookup(const ip_address& address);
        static async::task<expected<vector<string>, io::error>> async_reverse_lookup(const ip_address& address, async::stop_token stop = {});
    };
}
```

Names to addresses and back, through the system's resolver: `getaddrinfo` and `getnameinfo`, so `/etc/hosts`, the search domains, mDNS, the resolvers a VPN or a configuration profile installs, and the system's cache all apply. On macOS nothing else gives the right answer: the configuration is not in `/etc/resolv.conf` (Go calls the system's resolver there too). The call blocks inside the C library, so the async forms run it on the [blocking pool](../async/blocking.md), bounded by `config::blocking_threads`.

## Rules

- **A number is answered at once**, with no pool and no resolver: `lookup("10.0.0.1")`, `lookup("fe80::1%en0")`.
- **The order is the resolver's** (RFC 6724 where the system sorts), each address once.
- **Not found** is `net::errc::host_not_found` (`EAI_NONAME`), and so is an empty name; a failure of the resolver is its `EAI_*` code in [`net::lookup_category()`](error.md), whose `message()` is `gai_strerror`'s.
- **A stop** of the token ends the wait with `ECANCELED`, and wins over a result that comes at the same moment; a deadline (a `connect` limit) the same with `ETIMEDOUT`, and one at the clock's end is none. A job given up while it waited for a thread of the pool does not call the resolver at all; one already inside `getaddrinfo` cannot be stopped and finishes on its own, its result dropped, so a burst of lookups given up holds at most the pool's threads, for as long as the resolver takes (a name under `.local` that nobody answers: five seconds on macOS).
- **A name outside ASCII** is converted first (`txt::idna::to_ascii`): lookup does not guess the profile.
- No cache of its own, as in Go: the system caches. MX, SRV, TXT and a resolver of the module's own (RFC 1035) come with the Linux port.

## Members

```cpp
static expected<vector<ip_address>, io::error> lookup(const string& host);              // on this thread
static async::task<expected<vector<ip_address>, io::error>> async_lookup(const string& host, async::stop_token stop = {});
static expected<vector<string>, io::error> reverse_lookup(const ip_address& address);   // the system's name of the address
static async::task<expected<vector<string>, io::error>> async_reverse_lookup(const ip_address& address, async::stop_token stop = {});
```

```cpp
async::task<> show(string host) {
    async::stop_source src;
    src.stop_after(2s);                                 // a lookup of two seconds at most
    auto found = co_await net::dns::async_lookup(host, src.token());
    if (!found) {
        std::cerr << found.error().message() << '\n';   // "lookup nothing.invalid: no such host"
        co_return;
    }
    for (auto& a : *found) {
        std::cout << a.to_string() << '\n';
    }
}
```

## See also

- [socket](socket.md): `net::tcp::connect` looks the name up with the same stop and limit
- [blocking](../async/blocking.md), [stop_token](../async/stop_token.md)
- `tests/net/dial.cpp` (`NetDns_Tests`): `localhost`, numbers without the pool, RFC 6761 `.invalid`, a wait ended by its token and by its deadline

[sgcl](../../README.md) › [net](../README.md) › [dns](README.md)

# sgcl::net::dns::lookup_svcb, async_lookup_svcb

```cpp
static expected<vector<svcb>, io::error> lookup_svcb(const string& name);                                         // (1)
static expected<vector<svcb>, io::error> lookup_svcb(const string& name, const options& o);                       // (2)
static async::task<expected<vector<svcb>, io::error>> async_lookup_svcb(const string& name,                       // (3)
                                                                        async::stop_token stop = {}) noexcept;
static async::task<expected<vector<svcb>, io::error>> async_lookup_svcb(const string& name, const options& o,     // (4)
                                                                        async::stop_token stop = {}) noexcept;
```

Returns the SVCB records of `name` (RFC 9460): the endpoints of a service other than HTTPS, under the name its
protocol gives them — `"_dns.resolver.arpa"` for the encrypted resolvers of the local network (RFC 9462), or the
port and scheme prefixed to the host, `"_8443._foo.api.example.com"` (§2.3). Everything else is as for
[lookup_https](lookup_https.md): the records by priority, those of one priority in a random order, an alias followed
(eight at most), a record's `"."` made its owner's name, a malformed record rejecting the whole answer.

- (1, 3) The servers of `/etc/resolv.conf`; (2, 4) those of `o` ([dns::options](../dns-options.md)).
- (1–2) Wait for the lookup on the calling thread: they are for a thread (debug builds assert); a task awaits (3–4).
- (3–4) The same for a task, which holds no worker while it waits. A stop of `stop` ends the wait with `ECANCELED` at
  once.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name the service's protocol asks for, with its prefixes; a name with a trailing dot is absolute, tried as it is |
| `o` | the servers, the wait for one answer and the rounds over the servers ([dns::options](../dns-options.md)) |
| `stop` | ends the lookup when stopped; none by default |

## Return value

The [svcb](../dns-svcb.md) records, at least one, or the [io::error](../../io/error/README.md) of
[lookup_https](lookup_https.md#return-value), its path the name asked.

## Complexity

One query per server and attempt until one answers, and one more for each alias. No cache: every call asks.

## Exceptions

- (1–2) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (3–4) None.

## Example

The resolvers the local network designates (RFC 9462): each with its protocols and, for DNS over HTTPS, its path.

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    auto found = net::dns::lookup_svcb("_dns.resolver.arpa", {.servers = {"8.8.8.8"}});
    if (!found) {
        println("{}", found.error().message());
        return 1;
    }
    for (auto& r : *found) {
        println("{} {} alpn={} {}", r.priority, r.target, r.alpn.size(), r.dohpath);
    }
}
```

Sample output:

```text
1 dns.google. alpn=1
2 dns.google. alpn=2 /dns-query{?dns}
```

## See also

- [lookup_https, async_lookup_https](lookup_https.md): the HTTPS records
- [dns::svcb](../dns-svcb.md): what it gives
- [sgcl::net::dns](README.md)

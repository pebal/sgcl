[sgcl](../../README.md) › [net](../README.md) › [dns](README.md)

# sgcl::net::dns::lookup_cname, async_lookup_cname

```cpp
static expected<string, io::error> lookup_cname(const string& name);                                         // (1)
static expected<string, io::error> lookup_cname(const string& name, const options& o);                       // (2)
static async::task<expected<string, io::error>> async_lookup_cname(const string& name,                       // (3)
                                                                   async::stop_token stop = {}) noexcept;
static async::task<expected<string, io::error>> async_lookup_cname(const string& name, const options& o,     // (4)
                                                                   async::stop_token stop = {}) noexcept;
```

Returns the canonical name of `name`: the end of the chain of CNAME records that starts at it, or the name itself
when it is no alias (RFC 1034 §3.6.2), absolute either way: Go's `net.LookupCNAME`. The question asked is the
name's address (A), which a recursive server answers with the whole chain; a chain the server left unfollowed is
asked further, at most eight links in all.

- (1, 3) The servers of `/etc/resolv.conf`, its search list, `ndots`, `timeout` and `attempts`; (2, 4) the same with
  `o` in their place where it says so ([dns::options](../dns-options.md)): servers of its own over UDP, TCP, TLS or
  HTTPS, the wait for one answer, the rounds over the servers.
- (1–2) Run the lookup on the scheduler and wait for it on the calling thread: they are for a thread, as
  [task::wait](../../async/task/wait.md) is (debug builds assert); a task awaits (3–4).
- (3–4) The same for a task, which holds no worker while it waits; nothing starts before the task's first run. A stop
  of `stop` ends the wait with `ECANCELED` at once, closes the socket in use, and wins over an answer that comes at
  the same moment.

The lookup is the module's own stub resolver ([dns](README.md#rules)): a query over UDP to a server, the answer taken
only when it carries the query's random id and its question; over TCP when the answer comes truncated; over TLS or
HTTPS to a server of `o` that says so ([dns::server](../dns-server.md)); a name under `.local` by multicast DNS. A name
outside ASCII is converted by the caller first, with [txt::idna](../../txt/idna/README.md)`::to_ascii`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name; a name with a trailing dot is absolute, tried as it is |
| `o` | the servers, the wait for one answer and the rounds over the servers ([dns::options](../dns-options.md)) |
| `stop` | ends the lookup when stopped; none by default |

## Return value

The canonical name, absolute (`"github.com."`).
Or the [io::error](../../io/error/README.md), its operation `lookup` and its path `name`:

- `net::errc::host_not_found` for a name that does not exist (NXDOMAIN), and for one that is no name, asked of
  nobody: empty, two dots in a row, a label past 63 bytes, a name past 255;
- `net::errc::no_data` for a name that exists with no record of the type (NODATA, RFC 2308);
- `net::errc::server_failure` for a server's SERVFAIL, `net::errc::server_misbehaving` for its other refusals
  (REFUSED, FORMERR, NOTIMP), an answer that does not read, a referral it should have followed, a chain of CNAMEs
  past eight;
- `net::errc::invalid_address` for a server in `o` that is none ([dns::server](../dns-server.md)), `EINVAL` for a pin
  of one that is not base64 of 32 bytes;
- `ETIMEDOUT` (`is_timeout()`) when no attempt was answered, `ECANCELED` for the stop (3–4), the `errno` of a
  socket otherwise (`ECONNREFUSED`: nobody at the server's port), TLS's error of a server that does not authenticate
  (category `"tls"`), `EPROTONOSUPPORT` for a TLS or HTTPS server in a program without `sgcl/net/tls.h` or
  `sgcl/net/http.h`.

With a search list, the error is the one the name as given came to.

## Complexity

One query per server and attempt until one answers, for each name of the search list until one is found; an
answer truncated is asked again over TCP. No cache: every call asks.

## Exceptions

- (1–2) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (3–4) None.

## Notes

A name that exists without an IPv4 address (AAAA alone, MX alone) is its own canonical name, not `no_data`; a
chain whose last name does not exist gives that last name. Go's own resolver gives the first link of a longer
chain; its documentation, the RFC and this give the end.
## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    for (const char* name : {"www.github.com", "github.com"}) {
        auto canonical = net::dns::lookup_cname(name);
        println("{} {}", name, canonical ? *canonical : canonical.error().message());
    }
}
```

Sample output:

```text
www.github.com github.com.
github.com github.com.
```

Without the network, the failures a program meets first:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    println("{}", net::dns::lookup_cname("").error().message());

    net::dns::options bad;
    bad.servers = {"dns.example"};  // an address is needed, not a name
    println("{}", net::dns::lookup_cname("example.com", bad).error().message());

    net::udp::socket gone = net::udp::bind("127.0.0.1:0");
    net::dns::options closed;
    closed.servers = {gone.local_endpoint().to_string()};
    gone.close();  // nobody at that port: refused at once
    println("{}", net::dns::lookup_cname("example.com", closed).error().message());

    async::stop_source source;
    source.request_stop();
    auto stopped = net::dns::async_lookup_cname("example.com", source.token()).wait();
    println("{}", stopped.error().message());
}
```

Output:

```text
lookup: no such host
lookup dns.example: invalid address
lookup example.com: Connection refused
lookup example.com: Operation canceled
```

## See also

- [lookup, async_lookup](lookup.md): the addresses, through the system's resolver
- [dns::options](../dns-options.md): other servers, another wait
- [sgcl::net::dns](README.md)

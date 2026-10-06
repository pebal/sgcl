[sgcl](../../README.md) › [net](../README.md) › [dns](README.md)

# sgcl::net::dns::lookup_https, async_lookup_https

```cpp
static expected<vector<svcb>, io::error> lookup_https(const string& name);                                         // (1)
static expected<vector<svcb>, io::error> lookup_https(const string& name, const options& o);                       // (2)
static async::task<expected<vector<svcb>, io::error>> async_lookup_https(const string& name,                       // (3)
                                                                         async::stop_token stop = {}) noexcept;
static async::task<expected<vector<svcb>, io::error>> async_lookup_https(const string& name, const options& o,     // (4)
                                                                         async::stop_token stop = {}) noexcept;
```

Returns the HTTPS records of `name` (RFC 9460 §9): the endpoints of `https://name`, each with the protocols it
speaks, its port, addresses to try first and the key of Encrypted Client Hello, what a browser asks for beside the
addresses. Go's standard library has no such lookup. The records come ordered by priority, the lowest first, the
ones of one priority in a random order (§2.4.1). For a port other than 443 the name to ask is `"_8443._https.name"`
(§9.1).

An alias (AliasMode, priority 0) is followed: its target asked for its own records, eight aliases at most, and in an
answer that holds one the endpoints beside it are ignored, as §2.4.2 asks. An alias whose target has no records gives
one record of priority 0 whose target is that name: the client connects to it as if there were no record. An alias
to `"."` says the service is not offered. A CNAME in front of the records is followed too.

- (1, 3) The servers of `/etc/resolv.conf`, its search list, `ndots`, `timeout` and `attempts`; (2, 4) the same with
  `o` in their place where it says so ([dns::options](../dns-options.md)): servers of its own over UDP, TCP, TLS or
  HTTPS, the wait for one answer, the rounds over the servers.
- (1–2) Run the lookup on the scheduler and wait for it on the calling thread: they are for a thread, as
  [task::wait](../../async/task/wait.md) is (debug builds assert); a task awaits (3–4).
- (3–4) The same for a task, which holds no worker while it waits; nothing starts before the task's first run. A stop
  of `stop` ends the wait with `ECANCELED` at once, closes the socket in use, and wins over an answer that comes at
  the same moment.

The lookup is the module's own stub resolver ([dns](README.md#rules)), as for the other records. A name outside ASCII
is converted by the caller first, with [txt::idna](../../txt/idna/README.md)`::to_ascii`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the service, `"example.com"`, or with the port's prefix, `"_8443._https.example.com"`; a name with a trailing dot is absolute, tried as it is |
| `o` | the servers, the wait for one answer and the rounds over the servers ([dns::options](../dns-options.md)) |
| `stop` | ends the lookup when stopped; none by default |

## Return value

The [svcb](../dns-svcb.md) records, at least one: each target absolute, its SvcParams typed.
Or the [io::error](../../io/error/README.md), its operation `lookup` and its path the name asked:

- `net::errc::host_not_found` for a name that does not exist (NXDOMAIN), and for one that is no name, asked of
  nobody: empty, two dots in a row, a label past 63 bytes, a name past 255;
- `net::errc::no_data` for a name that exists with no HTTPS record (NODATA, RFC 2308), and for an alias to `"."`;
- `net::errc::server_failure` for a server's SERVFAIL; `net::errc::server_misbehaving` for its other refusals, an
  answer that does not read, a chain of CNAMEs or of aliases past eight, and a record that is malformed (§2.2: the
  keys not in increasing order, a value not of its key's form, a key of `mandatory` the record does not have, key
  65535), which rejects the whole answer;
- `net::errc::invalid_address` for a server in `o` that is none ([dns::server](../dns-server.md)), `EINVAL` for a pin
  of one that is not base64 of 32 bytes;
- `ETIMEDOUT` (`is_timeout()`) when no attempt was answered, `ECANCELED` for the stop (3–4), the `errno` of a
  socket otherwise (`ECONNREFUSED`: nobody at the server's port), TLS's error of a server that does not authenticate
  (category `"tls"`), `EPROTONOSUPPORT` for a TLS or HTTPS server in a program without `sgcl/net/tls.h` or
  `sgcl/net/http.h`.

## Complexity

One query per server and attempt until one answers, for each name of the search list until one is found, and one
more for each alias; an answer truncated is asked again over TCP. No cache: every call asks.

## Exceptions

- (1–2) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (3–4) None.

## Notes

A record whose `mandatory` names a key the program does not understand is returned all the same: the library reads
every key, the unknown ones into `params`, and leaves the choice to the program (§8 has a client skip such a record).
[tls::config](../tls/config.md)`::ech_from_dns` makes the client look the record up and take its `ech` by itself;
the lookup in clear text shows the name Encrypted Client Hello hides, so a program that wants the name kept asks over
TLS or HTTPS (`ech_dns`).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    auto found = net::dns::lookup_https("crypto.cloudflare.com");
    if (!found) {
        println("{}", found.error().message());
        return 1;
    }
    for (auto& r : *found) {
        println("{} {} alpn={} ech={} bytes", r.priority, r.target, r.alpn.size(), r.ech.size());
    }

    net::tls::config c;
    c.ech_from_dns = true;  // the client takes the record's ech by itself
    c.ech_dns.servers = {net::dns::cloudflare_https};
    auto conn = net::tls::connect("crypto.cloudflare.com:443", c);
    if (conn) {
        println("ech accepted: {}", net::tls::state_of(*conn)->ech_accepted);
    }
}
```

Sample output:

```text
1 crypto.cloudflare.com. alpn=2 ech=71 bytes
ech accepted: true
```

Without the network, the failures a program meets first:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    println("{}", net::dns::lookup_https("").error().message());

    net::dns::options bad;
    bad.servers = {"dns.example"};  // an address is needed, not a name
    println("{}", net::dns::lookup_https("example.com", bad).error().message());

    net::udp::socket gone = net::udp::bind("127.0.0.1:0");
    net::dns::options closed;
    closed.servers = {gone.local_endpoint().to_string()};
    gone.close();  // nobody at that port: refused at once
    println("{}", net::dns::lookup_https("example.com", closed).error().message());

    async::stop_source source;
    source.request_stop();
    auto stopped = net::dns::async_lookup_https("example.com", source.token()).wait();
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

- [dns::svcb](../dns-svcb.md): what it gives
- [lookup_svcb, async_lookup_svcb](lookup_svcb.md): the same for another service than HTTPS
- [tls::config](../tls/config.md): `ech_from_dns` and `ech_dns`
- [dns::options](../dns-options.md): other servers, another wait
- [sgcl::net::dns](README.md)

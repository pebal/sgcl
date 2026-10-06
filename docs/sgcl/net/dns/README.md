[sgcl](../../README.md) › [net](../README.md)

# sgcl::net::dns

```cpp
#include "sgcl/net/dns.h"   // or "sgcl/net.h"

namespace sgcl::net {
    struct dns {
        struct mx;
        struct srv;
        struct svcb;
        struct svc_param;
        struct server;
        struct options;
    };
}
```

`sgcl::net::dns` is names to addresses and back, and the records of a name. A structure of static functions, as
[tcp](../tcp/README.md) is; [tcp::connect](../tcp/connect.md) looks a name up through it. It has two resolvers,
and each question goes to the one that answers it right:

- The addresses go through the system's resolver: [lookup](lookup.md) is `getaddrinfo` and Go's `net.LookupHost`
  and `LookupIP`, [reverse_lookup](reverse_lookup.md) is `getnameinfo` and Go's `LookupAddr`. So `/etc/hosts`, the
  search domains, mDNS, the resolvers a VPN or a configuration profile installs, and the system's cache all apply; on
  macOS nothing else gives the right address, and Go calls the system's resolver there too. The call blocks inside
  the C library, so the `async_` forms run it on the [blocking pool](../../async/spawn_blocking.md), bounded by
  [config](../../core/config.md)`::blocking_threads`, and a stop token ends their wait.
- The records go through the module's own stub resolver (RFC 1035), as Go's resolver does with `PreferGo`:
  [lookup_mx](lookup_mx.md), [lookup_txt](lookup_txt.md), [lookup_srv](lookup_srv.md), [lookup_ns](lookup_ns.md)
  and [lookup_cname](lookup_cname.md), Go's `LookupMX`, `LookupTXT`, `LookupSRV`, `LookupNS` and `LookupCNAME`, and
  the service bindings of RFC 9460, [lookup_https](lookup_https.md) and [lookup_svcb](lookup_svcb.md), which Go's
  resolver does not ask for. The system's resolver has no portable way to ask for them. The queries go to the servers of `/etc/resolv.conf`, which
  macOS writes from its own configuration, or to those [options](../dns-options.md) names, over UDP on the
  scheduler's sockets, so a task that waits for one holds no thread at all. [lookup](lookup.md) and
  [reverse_lookup](reverse_lookup.md) given options go through it too.

A server of the options may be asked over TCP, over TLS (DNS over TLS, RFC 7858) or over HTTPS (DNS over HTTPS, RFC
8484), [dns::server](../dns-server.md) says how, so every lookup of the structure goes encrypted when the program
wants it to; the public resolvers of Cloudflare, Google and Quad9 are named by constants. A name under `.local` is
asked of the link by multicast DNS ([mdns](../mdns/README.md), RFC 6762) and of no server.

## Rules

- A number is answered at once by [lookup](lookup.md), with no pool and no resolver: `lookup("10.0.0.1")`,
  `lookup("fe80::1%en0")`. Its order is the resolver's (RFC 6724 where the system sorts), each address once.
- A name the resolver does not know is `net::errc::host_not_found` (`EAI_NONAME`, NXDOMAIN), and so is an empty
  name; a failure of the system's resolver is its `EAI_*` code in [lookup_category](../lookup_category.md); a name
  without a record of the type asked is `net::errc::no_data`, a server's SERVFAIL `net::errc::server_failure`, its
  other refusals `net::errc::server_misbehaving` ([errc](../errc.md)).
- A stop of an address lookup ends the wait, never the call: a job given up while it waited for a thread of the pool
  does not call the resolver at all, and one already inside `getaddrinfo` finishes on its own, its result dropped (a
  name under `.local` that nobody answers: five seconds on macOS). A stop of a record lookup ends it at once and
  closes its socket.
- From `/etc/resolv.conf` the stub resolver takes the servers (up to three; `port` of macOS's resolver(5)), the search
  list (`search` or `domain`, up to six; the domain of the host's name without one), `options ndots:n timeout:n
  attempts:n rotate use-vc`, as resolv.conf(5) caps them, and reads the file again when it changes (looked at once
  in five seconds at most). A file that names no server means the local machine's, `127.0.0.1` and `::1`, as
  resolv.conf(5) says.
- A record query over UDP is a socket of its own, connected to the server, on a port the system picks, with a random
  16-bit id and an OPT record of a 1232-byte UDP payload (RFC 6891); an answer counts only when it carries the id and
  echoes the question (RFC 5452), and anything else is ignored while the wait goes on. A truncated answer (TC), or a
  datagram longer than the payload, is asked again over TCP. NXDOMAIN and NODATA are answers; a server's failure, a
  timeout or a socket's error moves to the next server, `attempts` rounds over them.
- Over TLS a server's connection is kept and shared: the lookups that ask it at once have their queries in flight
  on it together, each with an id of its own among them, and their answers are taken in any order (RFC 7766
  §6.2.1.1); a connection idle for ten seconds is closed, one that fails is made again by the next query, its TLS
  session resumed. Over HTTPS the queries go through an [http::client](../http/client/README.md) of the resolver's
  own, HTTP/2 offered first, a query's id 0 (RFC 8484 §4.1). Both pad a query to a multiple of 128 bytes (RFC 7830,
  RFC 8467). The strict profile of RFC 8310 by default: a server that does not authenticate (its certificate for its
  name, or its pins) is the server's failure; [options](../dns-options.md)`::opportunistic` takes it unauthenticated,
  and asks in clear text one that has no TLS. TLS needs `sgcl/net/tls.h` in the program and HTTPS `sgcl/net/http.h`:
  those headers install the transports, so `sgcl/net/dns.h` needs neither.
- A name under `.local`, and the reverse names of link-local addresses (`254.169.in-addr.arpa.`, the `ip6.arpa.` of
  `fe80::/10`), are asked by multicast DNS on every interface (RFC 6762 §3, §4), whatever the servers: the first
  answer of a unique record ends the wait, NXDOMAIN when none came within the timeout (2 s by default).
- A name of a record is absolute, with its trailing dot (`"mx1.example.com."`), in the presentation form of RFC 1035
  §5.1: a dot inside a label is `\.`, a byte outside printable ASCII `\DDD`. A name asked may be written so too, and
  one with a trailing dot is tried as it is, without the search list. A CNAME in front of the records is followed, at
  most eight in all.
- A name outside ASCII is converted by the caller first, with [txt::idna](../../txt/idna/README.md)`::to_ascii`: dns
  does not guess the profile.
- No cache of its own, as in Go: the system caches the addresses; every record lookup asks (multicast DNS keeps the
  cache RFC 6762 asks for).

## Member types

| Type | Definition |
|---|---|
| [mx](../dns-mx.md) | a mail exchanger: its host and preference |
| [srv](../dns-srv.md) | a server of a service: its target, port, priority and weight |
| [svcb](../dns-svcb.md) | a service binding, an HTTPS or SVCB record: its priority, target and SvcParams (ALPN, port, hints, ECH) |
| [svc_param](../dns-svc_param.md) | a SvcParam of a key svcb has no field for: its key and value |
| [server](../dns-server.md) | a server to ask: its address with the transport (UDP, TCP, TLS, HTTPS), the name checked, the pins |
| [options](../dns-options.md) | how a lookup goes through the module's resolver: the servers, the wait for one answer, the rounds, the profile |

## Member objects

| Member | Description |
|---|---|
| `cloudflare`, `cloudflare_tls`, `cloudflare_https` | `"1.1.1.1"`, `"tls://1.1.1.1"`, `"https://cloudflare-dns.com/dns-query"` (static, `const char*`) |
| `google`, `google_tls`, `google_https` | `"8.8.8.8"`, `"tls://8.8.8.8"`, `"https://dns.google/dns-query"` (static, `const char*`) |
| `quad9`, `quad9_tls`, `quad9_https` | `"9.9.9.9"`, `"tls://9.9.9.9"`, `"https://dns.quad9.net/dns-query"` (static, `const char*`) |

## Member functions

#### Addresses

| Function | Description |
|---|---|
| [lookup, async_lookup](lookup.md) | the addresses of a name: the system's resolver, or the module's with options (static) |
| [reverse_lookup, async_reverse_lookup](reverse_lookup.md) | the names of an address: the system's resolver, or the module's PTR with options (static) |

#### Records

| Function | Description |
|---|---|
| [lookup_cname, async_lookup_cname](lookup_cname.md) | the canonical name of a name (static) |
| [lookup_https, async_lookup_https](lookup_https.md) | the HTTPS records of a name (RFC 9460), aliases followed, by priority (static) |
| [lookup_mx, async_lookup_mx](lookup_mx.md) | the mail exchangers of a domain, by preference (static) |
| [lookup_ns, async_lookup_ns](lookup_ns.md) | the name servers of a domain (static) |
| [lookup_srv, async_lookup_srv](lookup_srv.md) | the servers of a service, in RFC 2782's order (static) |
| [lookup_svcb, async_lookup_svcb](lookup_svcb.md) | the SVCB records of a service's name (RFC 9460), aliases followed, by priority (static) |
| [lookup_txt, async_lookup_txt](lookup_txt.md) | the TXT records of a name, each one text (static) |

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

- [tcp::connect](../tcp/connect.md): looks the name up with the same stop and limit
- [ip_address](../ip_address/README.md): what a lookup gives
- [spawn_blocking](../../async/spawn_blocking.md), [stop_token](../../async/stop_token/README.md)
- `tests/net/dial.cpp` (`NetDns_Tests`): `localhost`, numbers without the pool, RFC 6761 `.invalid`, a wait ended by
  its token and by its deadline
- `tests/net/dns_records.cpp`, `dns_message.cpp`, `dns_resolv_conf.cpp`, `dns_interop.cpp`: the records against a
  server on the loopback and its faults, the messages, `/etc/resolv.conf`, Go's resolver as the oracle
- `tests/net/dns_tls.cpp`, `tests/net/http/dns_https.cpp`: DNS over TLS and over HTTPS against servers on the
  loopback and Go's (`tools/dns_secure_oracle.go`)
- `tests/net/dns_https.cpp`, `tests/net/tls_ech_dns.cpp`: HTTPS and SVCB records against a server on the loopback,
  RFC 9460's vectors and Go's `dns/dnsmessage` (`tools/svcb_oracle.go`); ECH from the record
- [dns::server](../dns-server.md), [mdns](../mdns/README.md), [dns_sd](../dns_sd/README.md)

[sgcl](../README.md) › [net](README.md) › [dns](dns/README.md) › options

# sgcl::net::dns::options

```cpp
#include "sgcl/net/dns.h"   // or "sgcl/net.h"

namespace sgcl::net {
    struct dns {
        struct options {
            vector<dns::server> servers;
            duration timeout = {};
            int attempts = 0;
            bool opportunistic = false;
            bool https_get = false;
            string roots_pem;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::dns::options` is how a lookup goes through the module's own resolver, the second argument of
[lookup](dns/lookup.md) and [reverse_lookup](dns/reverse_lookup.md) (which then leave the system's resolver) and of
[lookup_mx](dns/lookup_mx.md), [lookup_txt](dns/lookup_txt.md), [lookup_srv](dns/lookup_srv.md),
[lookup_ns](dns/lookup_ns.md) and [lookup_cname](dns/lookup_cname.md): the servers to ask in place of the ones
`/etc/resolv.conf` names, each over UDP, TCP, TLS or HTTPS ([dns::server](dns-server.md)), how long to wait for one
answer, how many rounds over the servers, and for the encrypted transports the profile and the roots. What is left
at its default is the file's, the way Go's resolver reads the file and a `Dial` of a `net.Resolver` replaces the
servers alone: the search list and `ndots` stay the file's either way. A plain struct; a copy of its strings is a
copy of a word each, its vector copied.

## Member objects

| Member | Description |
|---|---|
| `servers` | the servers asked, in their order, in place of `/etc/resolv.conf`'s: each a [dns::server](dns-server.md), made from its text, `{"10.0.0.53", "tls://1.1.1.1"}`; an address with an optional port is asked over UDP (port 53, TCP for a truncated answer), `tcp://` over TCP alone, `tls://` over TLS (RFC 7858, port 853), `https://` a URL of DNS over HTTPS (RFC 8484); empty by default: the servers of `/etc/resolv.conf` |
| `timeout` | the wait for one answer of one server, its TCP exchange or its connection included; zero or less, the default: the file's `options timeout:n`, 5 s when it has none |
| `attempts` | the rounds over the servers, each server asked once in each; zero or less, the default: the file's `options attempts:n`, 2 when it has none |
| `opportunistic` | TLS without the authentication: RFC 8310's opportunistic profile, the server's certificate taken unchecked and its pins no condition, and a server that cannot be reached over TLS asked in clear text on port 53 of its address; false by default: the strict profile, a server that does not authenticate is the server's failure and the next is asked |
| `https_get` | DNS over HTTPS by GET, the query in base64url in the URL's `dns` (cacheable on the way), in place of POST; false by default |
| `roots_pem` | the certificates, as PEM, a TLS or HTTPS server's chain must lead to; empty by default: the system's roots |

## Example

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
    o.attempts = 2;
    auto found = net::dns::lookup_mx("example.com", o);
    println("{}", found.error().message());
    println("{}", found.error().is_timeout());

    o.servers = {"dns.example"};
    println("{}", net::dns::lookup_txt("example.com", o).error().message());
}
```

Output:

```text
lookup example.com: Operation timed out
true
lookup dns.example: invalid address
```

The public resolvers over TLS and over HTTPS, the first that answers taken (the internet is needed, and the program
includes `sgcl/net/tls.h` for TLS and `sgcl/net/http.h` for HTTPS, whose headers install the transports):

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::dns::options o;
    o.servers = {net::dns::cloudflare_tls, net::dns::google_https, net::dns::quad9};
    auto found = net::dns::lookup("example.com", o);
    if (!found) {
        println("{}", found.error().message());
        return 1;
    }
    for (auto& a : *found) {
        println("{}", a);
    }
}
```

## See also

- [dns::server](dns-server.md): one server and its transport
- [lookup_mx, async_lookup_mx](dns/lookup_mx.md) and the other lookups: what takes it
- [sgcl::net::dns](dns/README.md)

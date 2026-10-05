[sgcl](../../README.md) › [net](../README.md)

# sgcl::net::mdns

```cpp
#include "sgcl/net/mdns.h"   // or "sgcl/net.h"

namespace sgcl::net {
    struct mdns {
        struct options;
        class responder;
    };
}
```

`sgcl::net::mdns` is multicast DNS, RFC 6762: the names under `.local` answered by the hosts of the link themselves,
with no server. [lookup](lookup.md) asks the link for a host's addresses; a [responder](../mdns-responder/README.md)
answers for this host: its name, the addresses of each interface, and the services
[dns_sd](../dns_sd/README.md) publishes on it. It is a structure of static functions and the responder's handle, as
[dns](../dns/README.md) is, and a name under `.local` given to dns's own lookups goes here too. Go's standard
library has neither side; Apple's mDNSResponder and Avahi are system daemons a program talks to, where here the
program is the peer itself.

Both sides keep to the RFC: a query is multicast to `224.0.0.251` and `ff02::fb`, port 5353, on each interface chosen
(all that are up and can multicast by default), with a TTL of 255; a question asked again and again backs off from
one second to an hour, lists the answers its cache believes so that responders leave them out, and asks for a record
again before its TTL runs out; the cache keeps what comes, a goodbye or a cache-flush ending a record a second later.
A responder probes for its names before it announces them, renames one another host holds, answers at once for what
it alone owns and after a short random wait for what many share, and says goodbye when it closes.

## Rules

- The sockets are bound to port 5353 with `SO_REUSEADDR` and `SO_REUSEPORT`, so the system's responder (macOS's
  mDNSResponder, Avahi on Linux) keeps its own socket there and both work; it is never asked to stop. On macOS,
  mDNSResponder answers on the loopback interface too, so a program that keeps to it sees the services the system
  publishes, and the system sees the program's.
- A query of [lookup](lookup.md), [dns_sd::resolve](../dns_sd/resolve.md) or [dns_sd::types](../dns_sd/types.md)
  waits for the first answer of a record a host owns uniquely (an address, an SRV) or for the timeout of
  [options](../mdns-options.md), 2 s by default, asked again a second and three seconds after the first; nobody's
  answer within it is `net::errc::host_not_found`.
- The engines under the lookups and browses of one set of interfaces are shared by the program, made at the first
  and closed after the last; a responder has sockets of its own.
- A program sending multicast on a network interface may meet the system's local network permission (macOS 15); the
  loopback interface needs none.
- The interfaces are chosen as an engine starts: an interface that comes up later is not joined until the next one.

## Member types

| Type | Definition |
|---|---|
| [options](../mdns-options.md) | the interfaces and families, a lookup's wait, its QU question, a responder's host name |
| [responder](../mdns-responder/README.md) | a multicast DNS responder: this host's name, its addresses, the services published on it |

## Member functions

| Function | Description |
|---|---|
| [lookup, async_lookup](lookup.md) | the addresses of a host of the link (static) |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::mdns::options o;
    auto all = net::interfaces();
    for (auto& i : *all) {
        if (i.loopback) {
            o.interfaces.push_back(i);  // the loopback alone: nothing leaves the machine
        }
    }
    o.host = "docs-mdns";
    net::mdns::responder r = net::mdns::responder::start(o).value();
    println("{}", r.host_name());
    auto found = net::mdns::lookup("docs-mdns.local", o);
    println("{}", found->front());
    r.close();
}
```

Output:

```text
docs-mdns.local.
127.0.0.1
```

## See also

- [dns_sd](../dns_sd/README.md): the services of the link, browsed, resolved and published
- [dns](../dns/README.md): the lookups that send a name under `.local` here
- [udp::listen_multicast](../udp/listen_multicast.md): the sockets underneath
- `tests/net/mdns.cpp`: the querier against the responder on the loopback, probing and conflicts with two
  responders and a peer of the test's, goodbye, expiry, known answers, legacy unicast, dns-sd(1) both ways

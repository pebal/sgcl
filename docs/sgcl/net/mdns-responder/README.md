[sgcl](../../README.md) › [net](../README.md) › [mdns](../mdns/README.md) › responder

# sgcl::net::mdns::responder

```cpp
#include "sgcl/net/mdns.h"   // or "sgcl/net.h"

namespace sgcl::net {
    struct mdns {
        class responder;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::mdns::responder` is a multicast DNS responder (RFC 6762 §6, §8, §9): sockets of its own on the
interfaces of its [options](../mdns-options.md), this host's name probed for and announced with the addresses of each
interface, the services [published](publish.md) on it (their PTR, SRV and TXT, DNS-SD's records, RFC 6763), and every
query of the link for them answered. It is what Apple's mDNSResponder and Avahi are for their hosts, in the program:
Go has none in its standard library. [dns_sd::publish](../dns_sd/publish.md) makes one for a single service.

A responder is a handle of one word, a `tracked_ptr` to its state, as a [connection](../connection/README.md) is: a
copy is the same responder.

## Rules

- [start](start.md) and [publish](publish.md) wait for the probing: three queries a quarter of a second apart after a
  random wait of up to a quarter (§8.1), about a second. A name another host holds (a record of the name, type and
  class with other data, §9), or a simultaneous probe whose records are later in the order of §8.2, makes it take the
  next name: `"mymac-2"` for the host, `"Printer (2)"` for a service, and so on; after fifteen conflicts in ten seconds
  it waits five before it probes again.
- Announced twice, a second apart (§8.3); then each query is answered with its records less the ones it lists as
  known (§7.1), those of a name the responder alone owns at once and the shared ones (the PTRs) after 20 to 120 ms
  (§6), a PTR's SRV, TXT and addresses and an SRV's addresses in the additional section (RFC 6763 §12). A question
  that asks for a unicast answer (QU), or one from a port other than 5353 (a legacy querier, §6.7: its id and
  question echoed, TTLs of ten seconds at most), is answered to its sender. The host's addresses on an interface are
  that interface's.
- A conflict after the announcement takes the name back to probing (§9).
- [close](close.md) says goodbye, every record with TTL 0, and closes the sockets; a responder dropped without it
  closes them when the collector finds it, with no goodbye, and the link forgets its records when their TTL ends
  (two minutes for the host's, 75 for the PTRs).
- The interfaces are those of the start; an interface that comes up later is not joined.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](mdns-responder.md) | a handle that holds no responder; a copy |
| [start, async_start](start.md) | a responder started, its host's name held (static) |

#### Observers

| Function | Description |
|---|---|
| [host_name](host_name.md) | the host's name as it is held now |
| [services](services.md) | the services published, with their names as they are held now |
| [operator bool](operator_bool.md) | whether the handle holds a responder |

#### Modifiers

| Function | Description |
|---|---|
| [publish, async_publish](publish.md) | a service published, its name held |
| [remove, async_remove](remove.md) | a service withdrawn, its goodbye said |
| [close, async_close](close.md) | goodbye for every record, the sockets closed |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | whether two handles are the same responder |

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
            o.interfaces.push_back(i);
        }
    }
    o.host = "docs-responder";
    net::mdns::responder r = net::mdns::responder::start(o).value();
    net::dns_sd::service web;
    web.name = "Docs Web";
    web.type = "_http._tcp";
    web.port = 8080;
    web.txt = {{"path", "/"}};
    println("{}", r.publish(web).value());
    auto found = net::dns_sd::resolve("Docs Web", "_http._tcp", o);
    println("{}:{} {}", found->host, found->port, found->txt.get("path").value());
    r.close();
}
```

Output:

```text
Docs Web
docs-responder.local.:8080 /
```

## See also

- [dns_sd](../dns_sd/README.md): the services browsed, resolved and published
- [mdns::lookup](../mdns/lookup.md): the other side
- [sgcl::net::mdns](../mdns/README.md)

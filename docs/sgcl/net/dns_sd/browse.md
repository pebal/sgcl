[sgcl](../../README.md) › [net](../README.md) › [dns_sd](README.md)

# sgcl::net::dns_sd::browse

```cpp
expected<browser, io::error> browse(const string& type, const mdns::options& o = {}) noexcept;
```

Browses a service type of the link (RFC 6763 §4): a [browser](browser/README.md) whose `next` yields an
[event](event.md) as an instance comes or goes. The PTR of the type is asked at once (after 20 to 120 ms, RFC 6762
§5.2), then again a second later and each time twice as long after, up to an hour, with the instances it knows
listed so that their responders leave them out (§7.1); each instance is asked for again before its record runs out
(at 80, 85, 90 and 95% of its TTL). The instances the cache of the program already holds come first.

`type` is `"_http._tcp"` (the domain `local.`), `"_http._tcp.local."`, a subtype `"_printer._sub._ipp._tcp"`
(§7.1), or `"_services._dns-sd._udp"` for the types themselves (§9), whose events carry the type as their name. It
never waits, so it has no task form; the browse lasts until the browser is closed or dropped.

## Parameters

| Parameter | Description |
|---|---|
| `type` | the service type, a subtype, or `"_services._dns-sd._udp"` |
| `o` | the interfaces and families ([mdns::options](../mdns-options.md)) |

## Return value

The browser. Or the [io::error](../../io/error/README.md), its operation `browse`: `EINVAL` for a type that is none,
the error of the sockets when no interface could be joined.

## Complexity

A query on each interface and family at each round.

## Exceptions

None.

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
    net::dns_sd::service s;
    s.name = "Docs Browse";
    s.type = "_docs-browse._tcp";
    s.port = 9000;
    s.subtypes = {"_kiosk"};
    net::mdns::responder r = net::dns_sd::publish(s, o).value();
    for (const char* type : {"_docs-browse._tcp", "_kiosk._sub._docs-browse._tcp"}) {
        net::dns_sd::browser b = net::dns_sd::browse(type, o).value();
        auto e = b.next().value();
        println("{} {} {} {}", e.added, e.name, e.type, e.domain);
        b.close();
    }
    println("{}", net::dns_sd::browse("docs-browse", o).error().message());
    r.close();
}
```

Output:

```text
true Docs Browse _docs-browse._tcp local.
true Docs Browse _docs-browse._tcp local.
browse docs-browse: Invalid argument
```

## See also

- [browser](browser/README.md): what it gives
- [resolve](resolve.md): an instance's host and port
- [sgcl::net::dns_sd](README.md)

[sgcl](../../../README.md) › [net](../../README.md) › [dns_sd](../README.md)

# sgcl::net::dns_sd::browser

```cpp
#include "sgcl/net/mdns.h"   // or "sgcl/net.h"

namespace sgcl::net::dns_sd {
    class browser;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::dns_sd::browser` is a browse of a service type, what [browse](../browse.md) gives: the instances of the
link as they come and go, one [event](../event.md) at each [next](next.md), for as long as it is open. Under it the
type's PTR is asked again and again (RFC 6762 §5.2), the answers the program knows listed so that the responders
leave them out (§7.1), each record asked for again before it runs out, and an instance's goodbye or expiry told as
its removal. Apple's `DNSServiceBrowse` calls back; here the events wait in the browser until they are taken, so
none is lost between two calls.

A browser is a handle of one word, a `tracked_ptr` to its state: a copy is the same browse, and a `next` of one copy
takes the event from the others.

## Rules

- [next](next.md) waits for the next event; the task form takes a stop token.
- [close](close.md) ends the browse: no more queries, the events not taken dropped, a wait in `next` ended with
  `io::errc::closed`. A browser dropped without it ends when the collector finds it.
- The browses of one set of interfaces share the program's engine and its cache: a second browse of a type sees the
  instances the first one found at once.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](browser.md) | a handle that holds no browse; a copy |
| [next, async_next](next.md) | the next instance come or gone |
| [close](close.md) | the browse ended |
| [operator bool](operator_bool.md) | whether the handle holds a browse |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | whether two handles are the same browse |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<> watch(net::dns_sd::browser b) {
    async::stop_source stop;
    stop.stop_after(3s);
    for (int seen = 0; seen < 2;) {
        auto e = co_await b.async_next(stop.token());
        if (!e) {
            println("{}", e.error().message());
            co_return;
        }
        println("{} {}", e->added ? "+" : "-", e->name);
        ++seen;
    }
}

int main() {
    net::mdns::options o;
    auto all = net::interfaces();
    for (auto& i : *all) {
        if (i.loopback) {
            o.interfaces.push_back(i);
        }
    }
    net::dns_sd::browser b = net::dns_sd::browse("_docs-browser._tcp", o).value();
    net::dns_sd::service s;
    s.name = "Docs Browser";
    s.type = "_docs-browser._tcp";
    s.port = 1;
    net::mdns::responder r = net::dns_sd::publish(s, o).value();
    r.close();
    async::run(watch(b));
    b.close();
}
```

Output:

```text
+ Docs Browser
- Docs Browser
```

## See also

- [browse](../browse.md): what gives it
- [event](../event.md): what it gives
- [sgcl::net::dns_sd](../README.md)

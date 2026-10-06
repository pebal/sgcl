[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md)

# sgcl::net::nats::client

```cpp
#include "sgcl/net/nats/client.h"   // or "sgcl/net/nats.h"

namespace sgcl::net::nats {
    class client;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::nats::client` is a connection to a NATS server: [connect](connect.md) dials and sends CONNECT; then
[publish](publish.md), [subscribe](subscribe.md), [request](request.md) and [respond](respond.md). `nats.Conn` of
Go's nats.go.

## Rules

- A handle of one word: a copy is the same connection; every call may come from any task at once.
- A task of the module reads the connection and gives each message to its subscription; another writes what the
  publications buffered. PINGs go out every `ping_interval`; a server that leaves `max_pings_out` of them
  unanswered ends the connection as stale.
- There is no reconnection: a connection that ends ends the client; the program connects again.
- After [close](close.md), [drain](drain.md) or the connection's end, every call fails with the error that ended it.

## Member types

| Type | Definition |
|---|---|
| [options](../client-options.md) | how a client talks to its server |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](client.md) | no connection, or a copy of one |
| `(destructor)` | drops the handle |
| `operator=` | the handle of another connection |
| [connect, async_connect](connect.md) | a connection to a server |
| [publish, async_publish](publish.md) | a message to a subject |
| [subscribe, async_subscribe](subscribe.md) | a subject's messages |
| [request, async_request](request.md) | a request and its reply |
| [respond, async_respond](respond.md) | a reply to a message |
| [flush, async_flush](flush.md) | everything written read by the server |
| [drain, async_drain](drain.md) | the subscriptions ended, the connection closed |
| [close](close.md) | the connection closed at once |
| [server_id](server_id.md) | the server's id |
| [max_payload](max_payload.md) | the largest message the server takes |
| [operator bool](operator_bool.md) | whether the handle holds a connection |
| [operator==](operator_cmp.md) | whether two handles are the same connection |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/nats.h"

using namespace sgcl;

int main() {
    net::nats::client nc = net::nats::client::connect("nats://localhost:4222").value();
    net::nats::subscription sub = nc.subscribe("greetings.*").value();
    nc.publish("greetings.en", "Hello").value();
    nc.publish("greetings.pl", "Cześć").value();
    for (int i = 0; i < 2; ++i) {
        auto m = sub.receive().value();
        println("{}: {}", m.subject, m.data);
    }
}
```

Output:

```text
greetings.en: Hello
greetings.pl: Cześć
```

## See also

- [subscription](../subscription/README.md)
- [nats](../README.md)

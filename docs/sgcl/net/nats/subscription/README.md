[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md)

# sgcl::net::nats::subscription

```cpp
#include "sgcl/net/nats/client.h"   // or "sgcl/net/nats.h"

namespace sgcl::net::nats {
    class subscription;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::nats::subscription` is a subscription of a [client](../client/README.md)
([subscribe](../client/subscribe.md)): the [messages](../message/README.md) of its subject in the order the server
sent them, read by [receive](receive.md). `nats.Subscription` of nats.go read by `NextMsg`.

## Rules

- A handle of one word: a copy is the same subscription.
- It keeps `client::options::queue` messages; a message past them is dropped and counted by [dropped](dropped.md).
- It ends with [unsubscribe](unsubscribe.md) (at once, or after a count of its messages), the server's refusal of
  its subject, [drain](../client/drain.md) or the connection's end: what it received stays to be read, then receive
  fails with the reason.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](subscription.md) | no subscription, or a copy of one |
| `(destructor)` | drops the handle |
| `operator=` | the handle of another subscription |
| [receive, async_receive](receive.md) | the next message |
| [try_receive](try_receive.md) | the next message when one is there |
| [subject](subject.md) | the subject subscribed to |
| [queue_group](queue_group.md) | the queue group |
| [dropped](dropped.md) | the messages dropped |
| [unsubscribe, async_unsubscribe](unsubscribe.md) | no more messages |
| [operator bool](operator_bool.md) | whether the handle holds a subscription |
| [operator==](operator_cmp.md) | whether two handles are the same subscription |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/nats.h"

using namespace sgcl;

int main() {
    net::nats::client nc = net::nats::client::connect("nats://localhost:4222").value();
    net::nats::subscription sub = nc.subscribe("events.>").value();
    nc.flush().value();
    nc.publish("events.login", "alice").value();
    nc.publish("events.logout", "bob").value();
    for (int i = 0; i < 2; ++i) {
        auto m = sub.receive().value();
        println("{} {}", m.subject, m.data);
    }
}
```

Output:

```text
events.login alice
events.logout bob
```

## See also

- [subscribe](../client/subscribe.md)
- [message](../message/README.md)

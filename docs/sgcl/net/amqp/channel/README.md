[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md)

# sgcl::net::amqp::channel

```cpp
#include "sgcl/net/amqp/client.h"   // or "sgcl/net/amqp.h"

namespace sgcl::net::amqp {
    class channel;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::amqp::channel` is a channel of a [client](../client/README.md) (0-9-1 §2.2.5), where the work is done:
exchanges and queues declared and bound, messages published, consumed and got, deliveries acknowledged.
`amqp.Channel` of Go's amqp091-go.

## Rules

- A handle of one word: a copy is the same channel.
- Its synchronous methods (declarations, qos, get, consume, close) are taken one at a time; publications and
  acknowledgements may come from several tasks at once.
- A refusal of the broker's closes the channel: the operation's error is the broker's reply, and every later call
  on the channel gets it too. Another channel of the connection goes on.
- A delivery's `delivery_tag` belongs to the channel it came on: [ack](ack.md), [nack](nack.md) and
  [reject](reject.md) take it on that channel.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](channel.md) | no channel, or a copy of one |
| `(destructor)` | drops the handle |
| `operator=` | the handle of another channel |
| [id](id.md) | the channel's number |
| [close, async_close](close.md) | the channel ended |
| [operator bool](operator_bool.md) | whether the handle holds a channel |
| [operator==](operator_cmp.md) | whether two handles are the same channel |

#### Declarations

| Function | Description |
|---|---|
| [declare_exchange, async_declare_exchange](declare_exchange.md) | an exchange made, or checked |
| [delete_exchange, async_delete_exchange](delete_exchange.md) | an exchange deleted |
| [declare_queue, async_declare_queue](declare_queue.md) | a queue made, or checked |
| [bind_queue, async_bind_queue](bind_queue.md) | an exchange's messages to a queue |
| [unbind_queue, async_unbind_queue](unbind_queue.md) | a binding removed |
| [purge_queue, async_purge_queue](purge_queue.md) | a queue's messages dropped |
| [delete_queue, async_delete_queue](delete_queue.md) | a queue deleted |

#### Messages

| Function | Description |
|---|---|
| [publish, async_publish](publish.md) | a message to an exchange |
| [confirm, async_confirm](confirm.md) | publisher confirms from now on |
| [receive_returned, async_receive_returned](receive_returned.md) | a publication the broker gave back |
| [try_receive_returned](try_receive_returned.md) | the same, at once |
| [qos, async_qos](qos.md) | how many deliveries may wait for their acknowledgement |
| [consume, async_consume](consume.md) | a queue's messages to a consumer |
| [get, async_get](get.md) | a queue's next message |
| [ack, async_ack](ack.md) | a delivery done |
| [nack, async_nack](nack.md) | deliveries refused |
| [reject, async_reject](reject.md) | a delivery refused |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::client c = net::amqp::client::connect("amqp://guest:guest@localhost:5672/").value();
    net::amqp::channel ch = c.open_channel().value();
    ch.declare_exchange("logs", {.type = "fanout"}).value();
    auto q = ch.declare_queue("", {.exclusive = true}).value();
    ch.bind_queue(q.name, "logs", "").value();
    ch.publish("logs", "", "a log line").value();
    auto d = ch.get(q.name, true).value();
    println("{}", d->body);
}
```

Output:

```text
a log line
```

## See also

- [client](../client/README.md)
- [consumer](../consumer/README.md)

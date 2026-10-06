[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md)

# sgcl::net::amqp::client

```cpp
#include "sgcl/net/amqp/client.h"   // or "sgcl/net/amqp.h"

namespace sgcl::net::amqp {
    class client;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::amqp::client` is a connection to an AMQP 0-9-1 broker: [connect](connect.md) dials, logs in (PLAIN)
and opens the vhost; [open_channel](open_channel.md) opens the [channels](../channel/README.md) the work is done on.
`amqp.Connection` of Go's amqp091-go, its channels the same.

## Rules

- A handle of one word, as a connection is: a copy is the same connection.
- A task of the module reads the connection and gives each frame to its channel; heartbeats go both ways at the
  interval the two sides agreed.
- After [close](close.md), the broker's connection.close or a failure of the connection, every call fails with
  the error that ended it.

## Member types

| Type | Definition |
|---|---|
| [options](../client-options.md) | how a client talks to its broker |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](client.md) | no connection, or a copy of one |
| `(destructor)` | drops the handle |
| `operator=` | the handle of another connection |
| [connect, async_connect](connect.md) | a connection to a broker |
| [open_channel, async_open_channel](open_channel.md) | a channel of the connection |
| [blocked](blocked.md) | whether the broker stopped reading publications |
| [close, async_close](close.md) | the connection ended |
| [operator bool](operator_bool.md) | whether the handle holds a connection |
| [operator==](operator_cmp.md) | whether two handles are the same connection |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::client c = net::amqp::client::connect("amqp://guest:guest@localhost:5672/").value();
    net::amqp::channel ch = c.open_channel().value();
    ch.declare_queue("hello").value();
    ch.publish("", "hello", "Hello, World!").value();
    auto d = ch.get("hello").value();
    println("{}", d->body);
    ch.ack(d->delivery_tag);
    c.close();
}
```

Output:

```text
Hello, World!
```

## See also

- [channel](../channel/README.md)
- [amqp](../README.md)

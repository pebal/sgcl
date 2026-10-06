[sgcl](../../README.md) › [net](../README.md) › amqp

# sgcl::net::amqp

```cpp
#include "sgcl/net/amqp.h"   // namespace sgcl::net::amqp
```

A client of AMQP 0-9-1, RabbitMQ's protocol: what Go has in `amqp091-go` and Python in `pika`. A
[client](client/README.md) is a connection to a broker; its [channels](channel/README.md) declare exchanges and
queues, bind them, publish messages with their [properties](properties.md), consume them as
[deliveries](delivery.md) and acknowledge them. Publisher confirms make a publication wait for the broker's word
that it has it; a mandatory publication no queue takes comes back.

The frames are written and read by the module itself. A publication or an acknowledgement is buffered with the
ones beside it and written by one write, as the clients of other languages buffer; a method waited for (a
declaration, a get, a confirmed publication) is written at once. Heartbeats are kept both ways; a broker that stops
sending ends the connection.

## The rules

1. A URL names the broker: `amqp://user:password@host[:5672]/vhost` or `amqps://...:5671` (TLS); guest:guest and
   the vhost "/" when it has none.
2. A refusal of the broker's closes the channel it came on, as AMQP says: the operation's error carries the reply
   code ([errc](errc.md), 404 not found, 406 precondition failed) and the broker's text, and every later call on
   that channel gets it too; the connection and its other channels go on. A refusal on the connection
   (connection.close, 320 connection forced) ends the connection.
3. A [consumer](consumer/README.md) keeps its deliveries until they are received, `client::options::queue` of
   them; past it the connection waits (`qos` keeps the broker from sending more than the program acknowledges).
4. Every call that waits has two forms, `publish()` on a thread and `co_await async_publish()` in a task.
5. A [client](client/README.md), a [channel](channel/README.md) and a [consumer](consumer/README.md) are handles of
   one word; the structures hold strings.
6. Verified against the module's own test broker (`tests/net/amqp/server.h`) and the specification's bytes: no
   RabbitMQ or other broker is installed where it was written.

## Functions

| Function | Header | Description |
|---|---|---|
| [category](category.md) | `error.h` | the error category of `errc`, named `"amqp"` |
| [find](find.md) | `types.h` | a name's value in a field table |
| [make_error_code](make_error_code.md) | `error.h` | an `errc` as an `error_code` |

## Classes

| Class | Header | Description |
|---|---|---|
| [channel](channel/README.md) | `client.h` | a channel of a connection: declarations, publications, consumers, acknowledgements |
| [client](client/README.md) | `client.h` | a connection to a broker |
| [client::options](client-options.md) | `client.h` | TLS, heartbeats, frame size, channels, the timeout, the queue of deliveries |
| [consume_options](consume_options.md) | `types.h` | a consumer's tag, acknowledgement and exclusivity |
| [consumer](consumer/README.md) | `client.h` | a consumer's deliveries |
| [delivery](delivery.md) | `types.h` | a message as a consumer or get receives it |
| [exchange_options](exchange_options.md) | `types.h` | an exchange's type, durability and arguments |
| [field](field/README.md) | `types.h` | a value of a field table |
| [properties](properties.md) | `types.h` | what a message carries beside its body |
| [publish_options](publish_options.md) | `types.h` | mandatory |
| [queue_info](queue_info.md) | `types.h` | a declared queue's name, messages and consumers |
| [queue_options](queue_options.md) | `types.h` | a queue's durability, exclusivity and arguments |
| [returned](returned.md) | `types.h` | a publication the broker gave back |

## Types

| Type | Header | Description |
|---|---|---|
| `table` | `types.h` | `vector<pair<string, field>>`: a field table, the arguments of a declaration and the headers of a message, in their order |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [delivery_mode](delivery_mode.md) | `types.h` | in memory, or on disk too |
| [errc](errc.md) | `error.h` | the reply codes of AMQP and the client's own failures |

## See also

- [mqtt](../mqtt/README.md), [nats](../nats/README.md): the other brokers' clients
- [Benchmarks](../benchmarks.md)
- AMQP 0-9-1 (amqp0-9-1.pdf, the specification's XML), RabbitMQ's extensions; `tests/net/amqp/`

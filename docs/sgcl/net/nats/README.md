[sgcl](../../README.md) › [net](../README.md) › nats

# sgcl::net::nats

```cpp
#include "sgcl/net/nats.h"   // namespace sgcl::net::nats
```

A client of NATS: what Go has in `nats.go`. A [client](client/README.md) connects to a server, publishes
[messages](message/README.md) to subjects, [subscribes](client/subscribe.md) to them — "*" a token, ">" the rest, a
queue group sharing the messages among its members — and makes [requests](client/request.md) whose replies come to
an inbox of its own. Headers (NATS/1.0), user and password, a token, an nkey with its JWT, TLS.

Publications are buffered and written together by the client's writer, as NATS's clients do: a stream of them
costs an append each; [flush](client/flush.md) waits until the server has read everything before it. A request, a
reply and a subscription go out at once.

## The rules

1. A URL names the server: `nats://host[:4222]`, `nats://user:password@host`, `nats://token@host`, `tls://host`
   (TLS), or `host:port`; a server that requires TLS gets it.
2. NATS's core is at most once: a publication is written and gone, a message to a subscription whose queue is
   full is dropped (counted by [dropped](subscription/dropped.md)), as the clients of other languages drop them.
3. A refusal of the server's is an error of the category `"nats"` ([errc](errc.md)) with the server's text; one
   that is not about a subscription (an authorization, a payload past the limit) ends the connection.
4. Every call that waits has two forms, `request()` on a thread and `co_await async_request()` in a task.
5. A [client](client/README.md) and a [subscription](subscription/README.md) are handles of one word; a
   [message](message/README.md) holds strings.
6. Verified against the module's own test server (`tests/net/nats/server.h`) and the protocol's documentation: no
   nats-server is installed where it was written. JetStream is not part of the module.

## Functions

| Function | Header | Description |
|---|---|---|
| [category](category.md) | `error.h` | the error category of `errc`, named `"nats"` |
| [make_error_code](make_error_code.md) | `error.h` | an `errc` as an `error_code` |

## Classes

| Class | Header | Description |
|---|---|---|
| [client](client/README.md) | `client.h` | a connection to a server: publish, subscribe, request |
| [client::options](client-options.md) | `client.h` | the credentials, TLS, PINGs, the timeout, the queue of messages |
| [message](message/README.md) | `types.h` | a message: subject, reply subject, data, headers |
| [subscription](subscription/README.md) | `client.h` | a subscription's messages |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [errc](errc.md) | `error.h` | the failures of NATS |

## See also

- [amqp](../amqp/README.md), [mqtt](../mqtt/README.md): the other brokers' clients
- [Benchmarks](../benchmarks.md)
- docs.nats.io, "Client Protocol"; `tests/net/nats/`

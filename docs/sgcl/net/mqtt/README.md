[sgcl](../../README.md) › [net](../README.md) › mqtt

# sgcl::net::mqtt

```cpp
#include "sgcl/net/mqtt.h"   // namespace sgcl::net::mqtt
```

MQTT, the publish and subscribe protocol of devices and services (OASIS MQTT 5.0 and 3.1.1), both sides: a
[client](client/README.md) that connects to a broker over TCP, TLS or a WebSocket, publishes messages at each
quality of service, subscribes to topic filters and receives what matches, and a [broker](broker/README.md) that
keeps the sessions and routes each message to its subscribers. Go and Python leave MQTT to libraries (paho);
here it is one header, both versions on one codec.

What MQTT 5 adds is there: session expiry and session takeover, the will's delay, message expiry, topic aliases,
shared subscriptions (`$share/group/filter`), subscription identifiers, no-local and retain-as-published, the retain
handling, receive maximum and maximum packet size, user properties, request and response properties (response
topic, correlation data), the reason codes of every refusal. Under 3.1.1 the properties are not sent.

## The rules

1. A URL names the broker: `mqtt://host[:1883]`, `mqtts://host[:8883]` (TLS from the first byte),
   `ws://host/path` and `wss://host/path` (a WebSocket of subprotocol `mqtt`); the user and the password in it are
   the credentials when the [options](client-options.md) have none.
2. A [message](message/README.md) is a topic, a payload of bytes, a QoS, a retain flag and MQTT 5's properties. A
   topic has no wildcard; a filter may have `+` for one level and `#` for the rest; a topic starting with `$` is not
   matched by a wildcard at its first level.
3. QoS 0 is written and gone; [publish](client/publish.md) of QoS 1 returns at PUBACK, of QoS 2 at PUBCOMP, and a
   message received at QoS 2 is given to [receive](client/receive.md) once.
4. A refusal is an error of the category `"mqtt"` ([errc](errc.md)) whose path holds the reason code and its text
   ("0x87 Not authorized"); [reason_of](reason_of.md) reads the code back.
5. Every call that waits has two forms, `publish(...)` on a thread and `co_await async_publish(...)` in a task. A
   client is safe from many tasks at once: each call waits only for its own acknowledgement.
6. The broker keeps sessions, queued messages and retained messages in memory: they go with the process.

## Functions

| Function | Header | Description |
|---|---|---|
| [category](category.md) | `error.h` | the error category of `errc`, named `"mqtt"` |
| [make_error_code](make_error_code.md) | `error.h` | an `errc` as an `error_code` |
| [reason_of](reason_of.md) | `error.h` | the reason code an error carries |

## Classes

| Class | Header | Description |
|---|---|---|
| [broker](broker/README.md) | `broker.h` | an MQTT broker: sessions, routing, retained messages, wills |
| [client](client/README.md) | `client.h` | a session with a broker: publish, subscribe, receive |
| [client::options](client-options.md) | `client.h` | the client id, the credentials, the session, the will, the limits, TLS, the waits |
| [message](message/README.md) | `types.h` | an application message: topic, payload, QoS, retain, properties |
| [subscription](subscription.md) | `types.h` | a topic filter and how its messages come |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [errc](errc.md) | `error.h` | the failures of MQTT |
| [qos](qos.md) | `types.h` | at most once, at least once, exactly once |
| [retain_handling](retain_handling.md) | `types.h` | whether a subscription gets the retained messages |
| [version](version.md) | `types.h` | 3.1.1 or 5.0 |

## See also

- [http::websocket](../http/websocket/README.md): the transport of `ws://` and `wss://`
- [tls](../tls/README.md): the transport of `mqtts://`
- [Benchmarks](../benchmarks.md)
- OASIS MQTT Version 5.0 and Version 3.1.1; `tests/net/mqtt/` (a Python script that speaks MQTT packet by packet
  against the broker)

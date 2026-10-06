[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md)

# sgcl::net::mqtt::client

```cpp
#include "sgcl/net/mqtt/client.h"   // or "sgcl/net/mqtt.h"

namespace sgcl::net::mqtt {
    class client;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::mqtt::client` is a session with an MQTT broker: [connect](connect.md) dials and sends CONNECT, then
messages are [published](publish.md), filters [subscribed](subscribe.md) to and
[unsubscribed](unsubscribe.md), and what matches comes to [receive](receive.md) in the order it came. A reader task
takes every packet off the connection — the messages to a queue, the acknowledgements to the calls waiting on them —
and a keep-alive task sends PINGREQ when the session is quiet.

## Rules

- A handle of one word: a copy is the same session. Safe from many tasks and threads at once.
- The messages wait in a queue of `max_received` (the [options](../client-options.md)'); past it the reader waits,
  and the broker's flow control holds the rest.
- There is no reconnect: a session that ends is connected again by the program; `clean_start = false` and a
  `session_expiry` keep its subscriptions and the messages queued meanwhile at the broker.
- After [disconnect](disconnect.md), [close](close.md) or the end of the connection, every call is the session's end
  error.

## Member types

| Type | Definition |
|---|---|
| [options](../client-options.md) | how a client connects and what it asks of the broker |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](client.md) | no session, or a copy of one |
| `(destructor)` | drops the handle |
| `operator=` | the handle of another session |
| [connect, async_connect](connect.md) | a session with a broker |
| [publish, async_publish](publish.md) | a message to a topic |
| [subscribe, async_subscribe](subscribe.md) | subscriptions to filters |
| [unsubscribe, async_unsubscribe](unsubscribe.md) | subscriptions ended |
| [receive, async_receive](receive.md) | the next message of the subscriptions |
| [try_receive](try_receive.md) | the next message when one is there |
| [disconnect, async_disconnect](disconnect.md) | DISCONNECT and the end |
| [close](close.md) | the connection closed without DISCONNECT |
| [session_present](session_present.md) | whether the broker had a session of the client id |
| [client_id](client_id.md) | the client id |
| [is_connected](is_connected.md) | whether the session goes on |
| [operator bool](operator_bool.md) | whether the handle holds a session |
| [operator==](operator_cmp.md) | whether two handles are the same session |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/mqtt.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::mqtt::broker b;
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(b.async_serve(l));
    string url = string::concat("mqtt://", l.local_endpoint().to_string());
    net::mqtt::client c = net::mqtt::client::connect(url).value();
    c.subscribe("sensors/+/temp", net::mqtt::qos::at_least_once);
    c.publish("sensors/kitchen/temp", "21.5", net::mqtt::qos::at_least_once);
    net::mqtt::message m = c.receive().value();
    println("{} {}", m.topic, m.text());
    c.disconnect();
    b.close();
    serving.wait();
}
```

Output:

```text
sensors/kitchen/temp 21.5
```

## See also

- [broker](../broker/README.md)
- [mqtt](../README.md)

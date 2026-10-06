[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [client](README.md)

# sgcl::net::mqtt::client::publish, async_publish

```cpp
expected<void, io::error> publish(const string& topic, const string& payload,                           // (1)
                                  mqtt::qos q = mqtt::qos::at_most_once, bool retain = false) const;
async::task<expected<void, io::error>> async_publish(const string& topic, const string& payload,        // (2)
                                                     mqtt::qos q = mqtt::qos::at_most_once,
                                                     bool retain = false) const noexcept;
expected<void, io::error> publish(const message& m) const;                                              // (3)
async::task<expected<void, io::error>> async_publish(const message& m) const noexcept;                  // (4)
```

A message to a topic (MQTT 5 §3.3): QoS 0 written and gone, QoS 1 waited for until PUBACK, QoS 2 until
PUBCOMP (PUBREC, PUBREL, PUBCOMP). A retained message is kept by the broker for those who subscribe later; an empty
retained message deletes the one kept. Under MQTT 5 a topic sent again goes by its alias when the broker allows
aliases. QoS 1 and 2 publications in flight are at most the broker's receive maximum: one past it waits.

- (1–2) A text payload.
- (3–4) A [message](../message/README.md) with its properties.

## Parameters

| Parameter | Description |
|---|---|
| `topic` | the topic, without a wildcard |
| `payload` | the payload |
| `q` | the [qos](../qos.md) |
| `retain` | whether the broker keeps the message |
| `m` | the message |

## Return value

Nothing; `errc::topic_invalid` for a topic with a wildcard or none; `errc::publish_refused` for a refusal (0x87
not authorized, and without a packet sent: 0x9B a QoS past the broker's maximum, 0x9A retain not available);
`errc::packet_too_large` past the broker's maximum packet size; the session's end.

## Complexity

One packet for QoS 0, a round trip for QoS 1, two for QoS 2.

## Exceptions

- (1), (3) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2), (4) None.

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
    c.publish("config/mode", "eco", net::mqtt::qos::exactly_once, true);
    net::mqtt::client later = net::mqtt::client::connect(url).value();
    later.subscribe("config/#");
    net::mqtt::message m = later.receive().value();
    println("{} {} retained: {}", m.topic, m.text(), m.retain);
    c.disconnect();
    later.disconnect();
    b.close();
    serving.wait();
}
```

Output:

```text
config/mode eco retained: true
```

## See also

- [message](../message/README.md), [subscribe](subscribe.md)
- [client](README.md)

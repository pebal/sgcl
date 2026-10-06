[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md)

# sgcl::net::mqtt::message

```cpp
#include "sgcl/net/mqtt/types.h"   // or "sgcl/net/mqtt.h"

namespace sgcl::net::mqtt {
    struct message {
        string topic;
        vector<byte> payload;
        mqtt::qos qos = mqtt::qos::at_most_once;
        bool retain = false;
        duration expiry = {};
        bool utf8 = false;
        string content_type;
        string response_topic;
        vector<byte> correlation_data;
        vector<pair<string, string>> user_properties;
        vector<uint32_t> subscription_ids;
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::mqtt::message` is an application message, both ways: what [publish](../client/publish.md) sends and
[receive](../client/receive.md) gives. Its topic, payload, QoS and retain flag, and MQTT 5's properties — left empty
and not sent under 3.1.1. A plain value of its fields.

## Member objects

| Member | Description |
|---|---|
| `topic` | the topic |
| `payload` | the payload, bytes |
| `qos` | the [qos](../qos.md) it is published or delivered at |
| `retain` | kept by the broker for later subscribers (published); a retained message given at subscribe (received) |
| `expiry` | the Message Expiry Interval, whole seconds: a message not delivered within it is dropped; received, what is left of it; zero: none |
| `utf8` | the Payload Format Indicator: the payload is UTF-8 text (the broker checks it) |
| `content_type` | a MIME type of the payload |
| `response_topic` | where a request's answer goes |
| `correlation_data` | what ties an answer to its request |
| `user_properties` | names and values of the application's own, in their order |
| `subscription_ids` | received: the identifiers of the subscriptions it matched |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](message.md) | an empty message, or one of a topic and a text payload |
| [text](text.md) | the payload as text |
| [operator==](operator_cmp.md) | whether two messages are equal |

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
    c.subscribe("rpc/calc");
    net::mqtt::message m("rpc/calc", "{\"op\":\"add\",\"a\":2,\"b\":3}");
    m.content_type = "application/json";
    m.response_topic = "rpc/replies/42";
    m.user_properties.push_back({"trace", "abc"});
    c.publish(m);
    net::mqtt::message got = c.receive().value();
    println("{} {} {}", got.content_type, got.response_topic, got.user_properties[0].second);
    c.disconnect();
    b.close();
    serving.wait();
}
```

Output:

```text
application/json rpc/replies/42 abc
```

## See also

- [publish](../client/publish.md), [receive](../client/receive.md)
- [mqtt](../README.md)

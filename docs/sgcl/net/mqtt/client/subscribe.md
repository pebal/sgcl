[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [client](README.md)

# sgcl::net::mqtt::client::subscribe, async_subscribe

```cpp
expected<mqtt::qos, io::error> subscribe(const string& filter,                                                        // (1)
                                         mqtt::qos q = mqtt::qos::at_most_once) const;
async::task<expected<mqtt::qos, io::error>> async_subscribe(const string& filter,                                     // (2)
                                                            mqtt::qos q = mqtt::qos::at_most_once) const noexcept;
expected<vector<mqtt::qos>, io::error> subscribe(const vector<subscription>& s) const;                                // (3)
async::task<expected<vector<mqtt::qos>, io::error>> async_subscribe(vector<subscription> s) const noexcept;           // (4)
```

Subscriptions (MQTT 5 §3.8): the messages of topics that match the filters come to [receive](receive.md), at the
lower of their QoS and the QoS granted. A filter subscribed again replaces its subscription; the retained messages
of the filter come at once, as its retain handling says. `$share/<group>/<filter>` is a shared subscription: each
message goes to one member of the group.

- (1–2) One filter, the QoS granted.
- (3–4) [subscription](../subscription.md)s with their options, in one SUBSCRIBE (one per subscription identifier
  when they differ), the QoS granted to each. A filter refused is the call's error, the others staying subscribed.

## Parameters

| Parameter | Description |
|---|---|
| `filter` | the topic filter |
| `q` | the most QoS the client takes |
| `s` | the subscriptions |

## Return value

The QoS granted, or each one; `errc::topic_invalid` for a filter that is not one; `errc::subscribe_refused` for a
filter refused (0x87 not authorized, 0x8F filter invalid, 0x9E shared subscriptions not supported).

## Complexity

One round trip per subscription identifier.

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
    net::mqtt::subscription s;
    s.filter = "alerts/#";
    s.qos = net::mqtt::qos::exactly_once;
    s.identifier = 7;
    auto granted = c.subscribe(vector<net::mqtt::subscription>{s}).value();
    c.publish("alerts/fire", "evacuate", net::mqtt::qos::exactly_once);
    net::mqtt::message m = c.receive().value();
    println("{} {} {}", int(granted[0]), m.text(), m.subscription_ids[0]);
    c.disconnect();
    b.close();
    serving.wait();
}
```

Output:

```text
2 evacuate 7
```

## See also

- [subscription](../subscription.md), [unsubscribe](unsubscribe.md)
- [client](README.md)

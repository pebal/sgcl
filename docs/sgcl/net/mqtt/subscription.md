[sgcl](../../README.md) › [net](../README.md) › [mqtt](README.md) › subscription

# sgcl::net::mqtt::subscription

```cpp
#include "sgcl/net/mqtt/types.h"   // or "sgcl/net/mqtt.h"

namespace sgcl::net::mqtt {
    struct subscription {
        string filter;
        mqtt::qos qos = mqtt::qos::at_most_once;
        bool no_local = false;
        bool retain_as_published = false;
        mqtt::retain_handling retain_handling = mqtt::retain_handling::send;
        uint32_t identifier = 0;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::mqtt::subscription` is a topic filter and how its messages come (MQTT 5 §3.8.3.1), the element of
[subscribe](client/subscribe.md)'s list. Compared member by member (`==`).

## Member objects

| Member | Description |
|---|---|
| `filter` | the filter: `"sensors/+/temp"`, `"jobs/#"`, `"$share/workers/jobs/#"` |
| `qos` | the most QoS the subscriber takes |
| `no_local` | not the subscriber's own publications (MQTT 5; not for a shared subscription) |
| `retain_as_published` | the retain flag kept as published, not cleared for live messages (MQTT 5) |
| `retain_handling` | whether the retained messages come at subscribe: [retain_handling](retain_handling.md) |
| `identifier` | the Subscription Identifier its messages carry (MQTT 5); 0: none |

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
    s.filter = "chat/room1";
    s.no_local = true;
    c.subscribe(vector<net::mqtt::subscription>{s});
    c.publish("chat/room1", "my own words");
    println("{}", c.try_receive().has_value());
    c.disconnect();
    b.close();
    serving.wait();
}
```

Output:

```text
false
```

## See also

- [subscribe](client/subscribe.md)
- [mqtt](README.md)

[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [message](README.md)

# sgcl::net::mqtt::message::message

```cpp
message() = default;                                                                          // (1)
message(const string& topic, const string& payload, mqtt::qos q = mqtt::qos::at_most_once,    // (2)
        bool retain = false);
```

1. An empty message: no topic, no payload, QoS 0.
2. A message of the topic and a text payload (its bytes), the QoS and the retain flag; the properties empty.

## Parameters

| Parameter | Description |
|---|---|
| `topic` | the topic |
| `payload` | the payload |
| `q` | the QoS |
| `retain` | the retain flag |

## Complexity

Linear in the payload.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/mqtt.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::mqtt::message m("lights/hall", "on", net::mqtt::qos::at_least_once, true);
    println("{} {} {} {}", m.topic, m.payload.size(), int(m.qos), m.retain);
}
```

Output:

```text
lights/hall 2 1 true
```

## See also

- [message](README.md)

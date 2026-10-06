[sgcl](../../README.md) › [net](../README.md) › [mqtt](README.md)

# sgcl::net::mqtt::qos

```cpp
#include "sgcl/net/mqtt/types.h"   // or "sgcl/net/mqtt.h"

namespace sgcl::net::mqtt {
    enum class qos : uint8_t { at_most_once = 0, at_least_once = 1, exactly_once = 2 };
}
```

The quality of service of a delivery (MQTT 5 §4.3): how often a message may arrive and what it costs.

| Value | Description |
|---|---|
| `at_most_once` | QoS 0: written and gone, lost with the connection |
| `at_least_once` | QoS 1: acknowledged (PUBACK), sent again after a reconnect, may come twice |
| `exactly_once` | QoS 2: a handshake of four packets, once |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/mqtt.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    println("{}", int(net::mqtt::qos::exactly_once));
}
```

Output:

```text
2
```

## See also

- [mqtt](README.md)

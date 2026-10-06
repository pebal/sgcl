[sgcl](../../README.md) › [net](../README.md) › [mqtt](README.md)

# sgcl::net::mqtt::errc

```cpp
#include "sgcl/net/mqtt/error.h"   // or "sgcl/net/mqtt.h"

namespace sgcl::net::mqtt {
    enum class errc {
        refused = 1,
        publish_refused,
        subscribe_refused,
        disconnected,
        malformed_packet,
        protocol_error,
        packet_too_large,
        quota_exceeded,
        topic_invalid
    };
}
```

The failures of MQTT that neither `errno` nor [io](../../io/errc.md) names, in the category `"mqtt"` ([category](category.md)). An error of a refusal holds the reason code and its text in its path, "0x87 Not authorized" (and the server's reason string after a colon), which [reason_of](reason_of.md) reads back; 3.1.1's CONNACK return codes are given as MQTT 5's reason codes.

| Value | Description |
|---|---|
| `refused` | "the server refused the connection": CONNACK with a failure |
| `publish_refused` | "the publication was refused": PUBACK, PUBREC or PUBCOMP with a failure, or a QoS or retain the broker does not take |
| `subscribe_refused` | "the subscription was refused": SUBACK or UNSUBACK with a failure |
| `disconnected` | "disconnected by the peer": DISCONNECT from the other side |
| `malformed_packet` | "malformed MQTT packet" |
| `protocol_error` | "MQTT protocol error": a packet out of place |
| `packet_too_large` | "MQTT packet too large": past a maximum packet size |
| `quota_exceeded` | "MQTT quota exceeded": past a receive maximum |
| `topic_invalid` | "invalid MQTT topic": a topic with a wildcard or none, a filter that is not one |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/mqtt.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    error_code e = net::mqtt::errc::topic_invalid;
    println("{}: {}", e.category().name(), e.message());
}
```

Output:

```text
mqtt: invalid MQTT topic
```

## See also

- [mqtt](README.md)

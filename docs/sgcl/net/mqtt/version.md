[sgcl](../../README.md) › [net](../README.md) › [mqtt](README.md)

# sgcl::net::mqtt::version

```cpp
#include "sgcl/net/mqtt/types.h"   // or "sgcl/net/mqtt.h"

namespace sgcl::net::mqtt {
    enum class version : uint8_t { v3_1_1 = 4, v5 = 5 };
}
```

The protocol's version a [client](client/README.md) speaks; the [broker](broker/README.md) speaks both, each session in its client's.

| Value | Description |
|---|---|
| `v3_1_1` | MQTT 3.1.1 (OASIS 2014), protocol level 4 |
| `v5` | MQTT 5.0 (OASIS 2019): properties, reason codes, session expiry, aliases, shared subscriptions |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/mqtt.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    println("{}", int(net::mqtt::version::v3_1_1));
}
```

Output:

```text
4
```

## See also

- [mqtt](README.md)

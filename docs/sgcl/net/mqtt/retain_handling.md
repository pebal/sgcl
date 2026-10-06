[sgcl](../../README.md) › [net](../README.md) › [mqtt](README.md)

# sgcl::net::mqtt::retain_handling

```cpp
#include "sgcl/net/mqtt/types.h"   // or "sgcl/net/mqtt.h"

namespace sgcl::net::mqtt {
    enum class retain_handling : uint8_t { send = 0, send_if_new = 1, never = 2 };
}
```

Whether a [subscription](subscription.md) gets the retained messages of its filter at subscribe (MQTT 5 §3.8.3.1).

| Value | Description |
|---|---|
| `send` | always |
| `send_if_new` | only when the subscription did not exist |
| `never` | never: only what is published after |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/mqtt.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::mqtt::subscription s;
    s.retain_handling = net::mqtt::retain_handling::never;
    println("{}", int(s.retain_handling));
}
```

Output:

```text
2
```

## See also

- [mqtt](README.md)

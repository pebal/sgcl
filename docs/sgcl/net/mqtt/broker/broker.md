[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [broker](README.md)

# sgcl::net::mqtt::broker::broker

```cpp
broker() noexcept;                        // (1)
broker(const broker& other) = default;    // (2)
```

1. A broker with no sessions, the defaults of every field.
2. The same broker: the handle copied, the sessions shared.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the broker to share |

## Complexity

Constant.

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
    net::mqtt::broker b;
    net::mqtt::broker copy = b;
    println("{} {}", copy.connections(), int(copy.maximum_qos));
}
```

Output:

```text
0 2
```

## See also

- [serve](serve.md)
- [broker](README.md)

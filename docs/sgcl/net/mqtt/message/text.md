[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [message](README.md)

# sgcl::net::mqtt::message::text

```cpp
string text() const;
```

The payload as text, its bytes as they are.

## Parameters

None.

## Return value

The text.

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
    net::mqtt::message m("t", "21.5");
    println("{}", m.text());
}
```

Output:

```text
21.5
```

## See also

- [message](README.md)

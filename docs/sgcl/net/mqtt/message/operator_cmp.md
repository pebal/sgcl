[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [message](README.md)

# sgcl::net::mqtt::message::operator==

```cpp
friend bool operator==(const message&, const message&) noexcept = default;
```

Whether two messages are equal: every member.

## Parameters

None.

## Return value

`true` when every member is equal.

## Complexity

Linear in the messages.

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
    net::mqtt::message a("t", "x"), b("t", "x");
    println("{}", a == b);
    b.retain = true;
    println("{}", a == b);
}
```

Output:

```text
true
false
```

## See also

- [message](README.md)

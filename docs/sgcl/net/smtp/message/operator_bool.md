[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [message](README.md)

# sgcl::net::smtp::message::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle holds a message.

## Parameters

None.

## Return value

`false` for a default-constructed message.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    println("{}", bool(net::smtp::message()));
}
```

Output:

```text
false
```

## See also

- [message](README.md)

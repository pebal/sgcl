[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [client](README.md)

# sgcl::net::smtp::client::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle holds a session.

## Parameters

None.

## Return value

`false` for a default-constructed client.

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
    net::smtp::client none;
    println("{}", bool(none));
}
```

Output:

```text
false
```

## See also

- [client](README.md)

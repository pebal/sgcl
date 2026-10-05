[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [client](README.md)

# sgcl::net::smtp::client::client

```cpp
client() noexcept;                       // (1)
client(const client& other) noexcept;    // (2)
```

1. A client that holds no session: `!c` is `true`. One with a session is made by [connect](connect.md).
2. A handle of the session `other` holds.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the client whose session is shared |

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
    println("{}", !none);
}
```

Output:

```text
true
```

## See also

- [connect](connect.md)
- [client](README.md)

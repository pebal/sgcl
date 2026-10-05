[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [message](README.md)

# sgcl::net::smtp::message::message

```cpp
message() noexcept;                        // (1)
message(const message& other) noexcept;    // (2)
```

1. A message that holds none: `!m` is `true`. A server makes the ones its handler gets.
2. A handle of `other`'s message: the reading shared.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the message whose handle is copied |

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
    net::smtp::message none;
    println("{}", !none);
}
```

Output:

```text
true
```

## See also

- [message](README.md)

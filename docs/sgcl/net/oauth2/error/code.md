[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md) › [error](README.md)

# sgcl::net::oauth2::error::code

```cpp
string code() const noexcept;
```

The server's code (RFC 6749 §5.2, RFC 8628 §3.5, RFC 6750 §3.1): what a program decides by. `""` for a failure of the
exchange.

## Parameters

None.

## Return value

The code.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/oauth2.h"

using namespace sgcl;

int main() {
    net::oauth2::error e("authorization_pending", "");
    println("{}", e.code() == "authorization_pending");
}
```

Output:

```text
true
```

## See also

- [message](message.md)
- [error](README.md)

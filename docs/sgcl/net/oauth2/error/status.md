[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md) › [error](README.md)

# sgcl::net::oauth2::error::status

```cpp
int status() const noexcept;
```

The HTTP status the error came with: 400 for most codes, 401 for `invalid_client`; 0 for an error that came with no
response (a failure of the exchange, an error the client found itself).

## Parameters

None.

## Return value

The status, or 0.

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
    net::oauth2::error e("invalid_client", "", "", 401);
    println("{}", e.status());
}
```

Output:

```text
401
```

## See also

- [error](README.md)

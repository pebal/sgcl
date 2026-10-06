[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md) › [error](README.md)

# sgcl::net::oauth2::error::uri

```cpp
string uri() const noexcept;
```

The server's `error_uri`: a page about the error. `""` when it sent none.

## Parameters

None.

## Return value

The URI.

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
    net::oauth2::error e("invalid_client", "", "https://as.example/errors#client");
    println("{}", e.uri());
}
```

Output:

```text
https://as.example/errors#client
```

## See also

- [error](README.md)

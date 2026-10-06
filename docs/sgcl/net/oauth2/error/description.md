[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md) › [error](README.md)

# sgcl::net::oauth2::error::description

```cpp
string description() const noexcept;
```

The server's `error_description`: a sentence for a developer, not for the user. `""` when it sent none.

## Parameters

None.

## Return value

The description.

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
    net::oauth2::error e("invalid_scope", "admin is not granted to this client");
    println("{}", e.description());
}
```

Output:

```text
admin is not granted to this client
```

## See also

- [error](README.md)

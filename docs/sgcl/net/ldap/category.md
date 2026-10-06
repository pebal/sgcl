[sgcl](../../README.md) › [net](../README.md) › [ldap](README.md)

# sgcl::net::ldap::category

```cpp
const std::error_category& category() noexcept;
```

The error category of [errc](errc.md), named `"ldap"`; its messages are the result codes' names.

## Parameters

None.

## Return value

The category, one for the program.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ldap.h"

using namespace sgcl;

int main() {
    error_code e = net::ldap::errc::no_such_object;
    println("{} {}: {}", e.category().name(), e.value(), e.message());
}
```

Output:

```text
ldap 32: no such object
```

## See also

- [errc](errc.md)
- [make_error_code](make_error_code.md)

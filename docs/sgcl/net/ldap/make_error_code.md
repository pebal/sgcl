[sgcl](../../README.md) › [net](../README.md) › [ldap](README.md)

# sgcl::net::ldap::make_error_code

```cpp
error_code make_error_code(errc e) noexcept;
```

An [errc](errc.md) as an `error_code` of the [category](category.md) `"ldap"`; an `errc` converts to one by itself through it.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the code |


## Return value

The `error_code`.

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
    error_code e = net::ldap::make_error_code(net::ldap::errc::invalid_credentials);
    println("{} {}", e.value(), e.message());
}
```

Output:

```text
49 invalid credentials
```

## See also

- [errc](errc.md)
- [category](category.md)

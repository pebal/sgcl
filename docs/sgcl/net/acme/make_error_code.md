[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::make_error_code

```cpp
error_code make_error_code(errc e) noexcept;
```

The `error_code` of an [errc](errc.md), in [category](category.md): what `std::is_error_code_enum` uses for the
comparisons `e.code() == net::acme::errc::rate_limited`.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the code |

## Return value

The error code.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    error_code c = net::acme::make_error_code(net::acme::errc::unauthorized);
    println("{} {}", c.category().name(), c.message());
}
```

Output:

```text
acme acme: unauthorized
```

## See also

- [errc](errc.md)
- [net::acme](README.md)

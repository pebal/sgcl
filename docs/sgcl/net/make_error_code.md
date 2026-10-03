[sgcl](../README.md) › [net](README.md)

# sgcl::net::make_error_code

```cpp
#include "sgcl/net/error.h"   // or "sgcl/net.h"

namespace sgcl::net {
    error_code make_error_code(errc e) noexcept;
}
```

Returns the `error_code` of a code of the module: `e` in the [category](category.md) of net. It is the function
`std::error_code` finds by argument-dependent lookup when an [errc](errc.md) converts to it, so an `errc` converts
to an `error_code` and compares with one by itself, as `std::make_error_code` serves `std::errc`.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the code of the module |

## Return value

The `error_code` of value `e` in the category of net.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    error_code code = net::make_error_code(net::errc::http_status);
    println("{} {} {}", code.value(), code.category().name(), code.message());
    println("{}", code == net::errc::http_status);
}
```

Output:

```text
12 net the response's status is not 2xx
true
```

## See also

- [errc](errc.md), [category](category.md)
- [io::error](../io/error/README.md)

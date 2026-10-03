[sgcl](../README.md) › [io](README.md)

# sgcl::io::make_error_code

```cpp
#include "sgcl/io/error.h"   // or "sgcl/io.h"

namespace sgcl::io {
    error_code make_error_code(errc e) noexcept;
}
```

Returns the `error_code` of a code of the module: `e` in the [category](category.md) of io. It is the function
`std::error_code` finds by argument-dependent lookup when an [errc](errc.md) converts to it, so an `errc` converts
to an `error_code` and compares with one by itself, as `std::make_error_code` serves `std::errc`.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the code of the module |

## Return value

The `error_code` of value `e` in the category of io.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    error_code code = io::make_error_code(io::errc::insecure_path);
    println("{} {} {}", code.value(), code.category().name(), code.message());
    println("{}", code == io::errc::insecure_path);
}
```

Output:

```text
11 io insecure path
true
```

## See also

- [errc](errc.md), [category](category.md)
- [error](error.md)

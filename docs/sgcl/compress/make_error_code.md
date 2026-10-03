[sgcl](../README.md) › [compress](README.md)

# sgcl::compress::make_error_code

```cpp
#include "sgcl/compress/error.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    error_code make_error_code(errc e) noexcept;
}
```

Makes the `error_code` of a code of the module, in the [compress category](compress_category.md). It is found by
argument-dependent lookup, and `errc` is an error code enumeration, so an `errc` converts to an `error_code` and
compares with one by itself: `ec == compress::errc::checksum`.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the code |

## Return value

The `error_code` of the value of `e` in the compress category.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    error_code ec = compress::make_error_code(compress::errc::too_large);
    println("{}: {} ({})", ec.category().name(), ec.message(), ec.value());
    println("{}", ec == compress::errc::too_large);
}
```

Output:

```text
compress: size limit exceeded (5)
true
```

## See also

- [compress_category](compress_category.md): the category
- [errc](errc.md): the codes
- [sgcl::compress](README.md)

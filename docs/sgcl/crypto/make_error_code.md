[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::make_error_code

```cpp
#include "sgcl/crypto/error.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    error_code make_error_code(errc e) noexcept;
}
```

Returns the code `e` as a `std::error_code` of the category [crypto_category](crypto_category.md), for code that
speaks in error codes. `std::error_code`'s constructor from an enumeration finds it by itself, since
`std::is_error_code_enum<errc>` is true, so `std::error_code code = crypto::errc::malformed;` calls it; an
`error_code` compared with a code of [errc](errc.md) goes through it too.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the code |

## Return value

A `std::error_code` whose value is the code's and whose category is `crypto_category()`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

#include <system_error>

using namespace sgcl;

int main() {
    std::error_code code = crypto::make_error_code(crypto::errc::authentication);
    println("{} {} {}", code.category().name(), code.value(), code.message());
    println("{} {}", code == crypto::errc::authentication, code == crypto::errc::malformed);
    println("{}", bool(code));
}
```

Output:

```text
crypto 1 message authentication failed
true false
true
```

## See also

- [crypto_category](crypto_category.md): the category
- [errc](errc.md): the codes
- [error](error/README.md): the error of the module

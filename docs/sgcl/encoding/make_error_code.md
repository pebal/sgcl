[sgcl](../README.md) › [encoding](README.md)

# sgcl::encoding::make_error_code

```cpp
#include "sgcl/encoding/error.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    error_code make_error_code(errc e) noexcept;
}
```

The `error_code` of an [errc](errc.md): its value and the category [encoding_category()](encoding_category.md). The
standard library finds it by the argument's namespace, and `std::is_error_code_enum<errc>` is true, so an `errc`
converts to an `error_code` without it being called by name: `error_code code = encoding::errc::syntax`, and a
comparison `code == encoding::errc::syntax`. A stream of a program's own that fails on what it decodes reports the
code so, as the decoders of the module do.

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
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    error_code code = encoding::make_error_code(encoding::errc::unexpected_end);
    println("{} {} {}", code.value(), code.category().name(), code.message());

    io::error failed(encoding::errc::invalid_character, "decode", "my format");
    println("{}", failed.message());
    println("{}", failed.code() == encoding::errc::invalid_character);
}
```

Output:

```text
2 encoding unexpected end of input
decode my format: invalid character
true
```

## See also

- [encoding_category](encoding_category.md): the category
- [io::error](../io/error.md): a stream's error, which carries the code
- [sgcl::encoding](README.md)

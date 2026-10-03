[sgcl](../README.md) › [encoding](README.md)

# sgcl::encoding::encoding_category

```cpp
#include "sgcl/encoding/error.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    const std::error_category& encoding_category() noexcept;
}
```

The `std::error_category` of the codes of [errc](errc.md), named `"encoding"`: the category of the `error_code`
an [io::error](../io/error.md) carries when a decoder read as a stream fails on its input. One object for the whole
program, so that a code's category compares with it by address. Its `message(value)` gives a code's words,
`"invalid character"`, and `"unknown encoding error"` for a value that is not one of the list.

## Parameters

None.

## Return value

The category.

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
    const std::error_category& category = encoding::encoding_category();
    println("{}", category.name());
    println("{}", category.message(int(encoding::errc::field_count)));
    println("{}", category.message(99));

    error_code code = encoding::errc::syntax;
    println("{}", code.category() == category);
}
```

Output:

```text
encoding
wrong number of fields
unknown encoding error
true
```

## See also

- [make_error_code](make_error_code.md): an `errc` as an `error_code` of this category
- [errc](errc.md): the codes
- [sgcl::encoding](README.md)

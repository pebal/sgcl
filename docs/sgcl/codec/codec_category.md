[sgcl](../README.md) › [codec](README.md)

# sgcl::codec::codec_category

```cpp
#include "sgcl/codec/error.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    const std::error_category& codec_category() noexcept;
}
```

The `std::error_category` of the [errc](errc.md) codes: an `errc` as a `std::error_code` is a value of this
category, as `std::generic_category()` is the category of `std::errc`. Its `name()` is `"codec"`, and its
`message(c)` the words of the code: `"corrupt image data"`, `"checksum mismatch"`, `"unexpected end of data"`,
`"unsupported feature"`, `"size limit exceeded"`, `"invalid argument"`, `"input/output error"`, and
`"unknown codec error"` for a value outside the list. The category is one object for the whole program, so
`error_code`s compare by its address.

## Parameters

None.

## Return value

A reference to the category, the same object at every call.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    const std::error_category& category = codec::codec_category();
    println("{}", category.name());
    println("{}", category.message(int(codec::errc::too_large)));
    std::error_code code = codec::errc::too_large;
    println("{}", code.category() == category);
}
```

Output:

```text
codec
size limit exceeded
true
```

## See also

- [errc](errc.md): the codes
- [make_error_code](make_error_code.md): an `errc` as a `std::error_code`
- [sgcl::codec](README.md)

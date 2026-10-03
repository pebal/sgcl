[sgcl](../README.md) › [codec](README.md)

# sgcl::codec::make_error_code

```cpp
#include "sgcl/codec/error.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    error_code make_error_code(errc e) noexcept;
}
```

The [errc](errc.md) `e` as a `std::error_code` of [codec_category](codec_category.md), its value the code's number.
It is what the conversion of an `errc` to a `std::error_code` calls, found by argument-dependent lookup since
`std::is_error_code_enum<codec::errc>` is specialized: `std::error_code code = codec::errc::checksum;` and
`code == codec::errc::checksum` need no call of it written.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the code |

## Return value

The `std::error_code`.

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
    std::error_code code = codec::make_error_code(codec::errc::corrupt);
    println("{} {}: {}", code.category().name(), code.value(), code.message());
    println("{}", code == codec::errc::corrupt);
    println("{}", bool(std::error_code()));
}
```

Output:

```text
codec 1: corrupt image data
true
false
```

## See also

- [errc](errc.md): the codes
- [codec_category](codec_category.md): their category
- [sgcl::codec](README.md)

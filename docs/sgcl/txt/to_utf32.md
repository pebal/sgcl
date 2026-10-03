[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::to_utf32

```cpp
#include "sgcl/txt/encoding.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    vector<char32_t> to_utf32(const string& text) noexcept;
}
```

Returns the code points of the text `text`, for the code that wants a code point to be an integer. An invalid sequence of UTF-8 is a `U+FFFD`.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |

## Return value

The code points.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{::#x}", txt::to_utf32("a😀"));
}
```

Output:

```text
[0x61, 0x1f600]
```

## See also

- [from_utf32](from_utf32.md): the way back
- [runes](../core/utf8.md): the code points without a vector
- [sgcl::txt](README.md)

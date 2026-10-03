[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::from_utf32

```cpp
#include "sgcl/txt/encoding.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    string from_utf32(const slice<const char32_t>& points);
}
```

Returns the text of the code points `points`, as UTF-8. A value that is no code point — a surrogate, or one past `U+10FFFF` — is a `U+FFFD`.

## Parameters

| Parameter | Description |
|---|---|
| `points` | the code points |

## Return value

The text.

## Complexity

Linear in the number of code points: the widths summed, then the text written once.

## Exceptions

`length_error` when the result would pass a string's `max_size()`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    char32_t points[] = {U'x', char32_t(0x1F600), char32_t(0x110000)};
    for (char32_t c : txt::from_utf32(points).runes()) {
        println("U+{:04X}", uint32_t(c));
    }
}
```

Output:

```text
U+0078
U+1F600
U+FFFD
```

## See also

- [to_utf32](to_utf32.md): the way back
- [sgcl::txt](README.md)

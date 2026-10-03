[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::from_utf16

```cpp
#include "sgcl/txt/encoding.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    string from_utf16(const slice<const char16_t>& units);
}
```

Returns the text the UTF-16 units `units` stand for, as UTF-8. A surrogate on its own, not one of a pair, is a `U+FFFD`.

## Parameters

| Parameter | Description |
|---|---|
| `units` | the units, in the byte order of the machine |

## Return value

The text.

## Complexity

Linear in the number of units.

## Exceptions

`length_error` when the units are more than a third of a string's `max_size()`: `sgcl::txt::from_utf16: a text longer than a string can hold`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    char16_t units[] = {u'z', char16_t(0xD83D), char16_t(0xDE00), char16_t(0xD800)};
    string text = txt::from_utf16(units);
    for (char32_t c : text.runes()) {
        println("U+{:04X}", uint32_t(c));
    }
}
```

Output:

```text
U+007A
U+1F600
U+FFFD
```

## See also

- [to_utf16](to_utf16.md): the way back
- [decode](decode.md): UTF-16 as bytes
- [sgcl::txt](README.md)

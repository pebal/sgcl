[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::grapheme_next

```cpp
#include "sgcl/txt/segment.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    size_t grapheme_next(const slice<const char>& text, size_t pos) noexcept;    // (1)
    size_t grapheme_next(const string& text, size_t pos) noexcept;               // (2)
    template<size_t N>
    size_t grapheme_next(const char (&text)[N], size_t pos) noexcept;            // (3)
    template<class P>
    requires std::same_as<P, const char*> || std::same_as<P, char*>
    size_t grapheme_next(P text, size_t pos) noexcept;                           // (4)
}
```

Returns the grapheme boundary after the byte position `pos`, by [UAX #29](https://www.unicode.org/reports/tr29/): what
a caret and a right arrow move by, never half a character, however many code points it is. From a position inside a
grapheme it goes to that grapheme's end, so a cursor that starts astray is put right. A position at or past the end is
the end.

1. In a slice of UTF-8 bytes.
2. In a string.
3. In an array of `char`, a literal among them, read where it lies up to its first NUL or its end.
4. In a C text, `const char*` or `char*`, read where it lies up to its NUL.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `pos` | a byte position in it |

## Return value

The byte position of the end of the grapheme that holds `pos`; the size of the text when `pos` is at or past its end.

## Complexity

Linear in `pos` and the grapheme after it: the boundaries are found from the start of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string text = "Zażółć gęślą jaźń — a potem 漢字 i 🇵🇱 na koniec.";
    size_t caret = text.find("🇵🇱");
    println("{} -> {}", caret, txt::grapheme_next(text, caret));  // the flag is one character
    println("{}", txt::grapheme_next(text, caret + 1));
}
```

Output:

```text
48 -> 56
56
```

## See also

- [grapheme_prev](grapheme_prev.md): the boundary before
- [grapheme_start](grapheme_start.md): a position put back on its character's start
- [txt](README.md)

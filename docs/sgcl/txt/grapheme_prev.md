[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::grapheme_prev

```cpp
#include "sgcl/txt/segment.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    size_t grapheme_prev(const slice<const char>& text, size_t pos) noexcept;    // (1)
    size_t grapheme_prev(const string& text, size_t pos) noexcept;               // (2)
    template<size_t N>
    size_t grapheme_prev(const char (&text)[N], size_t pos) noexcept;            // (3)
    template<class P>
    requires std::same_as<P, const char*> || std::same_as<P, char*>
    size_t grapheme_prev(P text, size_t pos) noexcept;                           // (4)
}
```

Returns the last grapheme boundary before the byte position `pos`, by [UAX
#29](https://www.unicode.org/reports/tr29/): what a left arrow and a backspace move by, never half a character,
however many code points it is. From a position inside a grapheme it goes to that grapheme's start, so a cursor that
starts astray is put right. A position past the end is taken as the end; the boundary before 0 is 0.

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

The byte position of the boundary before `pos`; 0 when `pos` is 0.

## Complexity

Linear in `pos`: the text is scanned from its start, the rules of UAX #29 running one way only. A slice is usually a
line, and a cursor is not a loop.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string s = "ae\u0301\U0001F1F5\U0001F1F1";  // a, e and an acute, a flag
    size_t caret = s.size();
    while (caret > 0) {
        caret = txt::grapheme_prev(s, caret);  // a backspace
        print("{} ", caret);
    }
    println();
    println("{} {}", txt::grapheme_prev(s, 2), txt::grapheme_prev(s, 0));
}
```

Output:

```text
4 1 0 
1 0
```

## See also

- [grapheme_next](grapheme_next.md): the boundary after
- [grapheme_start](grapheme_start.md): a position put back on its character's start
- [txt](README.md)

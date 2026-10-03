[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::grapheme_start

```cpp
#include "sgcl/txt/segment.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    size_t grapheme_start(const slice<const char>& text, size_t pos) noexcept;    // (1)
    size_t grapheme_start(const string& text, size_t pos) noexcept;               // (2)
    template<size_t N>
    size_t grapheme_start(const char (&text)[N], size_t pos) noexcept;            // (3)
    template<class P>
    requires std::same_as<P, const char*> || std::same_as<P, char*>
    size_t grapheme_start(P text, size_t pos) noexcept;                           // (4)
}
```

Returns the start of the grapheme that holds the byte position `pos`, by [UAX
#29](https://www.unicode.org/reports/tr29/): `pos` itself when it is a boundary. It puts a position that fell inside a
character, a byte of a code point or a code point of a grapheme, back on the character's start, so a cursor never
stands in half a character. A position past the end is the end.

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

The byte position of the start of the grapheme that holds `pos`; the size of the text when `pos` is at or past its
end.

## Complexity

Linear in `pos`: the boundaries are found from the start of the text, the rules of UAX #29 running one way only.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string s = "ae\u0301\U0001F1F5\U0001F1F1";  // a, e and an acute, a flag
    for (size_t pos : {0, 1, 2, 3, 4, 7, 11, 20}) {
        print("{}->{} ", pos, txt::grapheme_start(s, pos));
    }
    println();
}
```

Output:

```text
0->0 1->1 2->1 3->1 4->4 7->4 11->4 20->12 
```

## See also

- [grapheme_next](grapheme_next.md), [grapheme_prev](grapheme_prev.md): the cursor moved
- [graphemes](graphemes.md): the graphemes of a text
- [txt](README.md)

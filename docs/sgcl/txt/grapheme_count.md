[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::grapheme_count

```cpp
#include "sgcl/txt/segment.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    size_t grapheme_count(const slice<const char>& text) noexcept;     // (1)
    size_t grapheme_count(const string& text) noexcept;                // (2)
    template<size_t N>
    size_t grapheme_count(const char (&text)[N]) noexcept;             // (3)
    template<class P>
    requires std::same_as<P, const char*> || std::same_as<P, char*>
    size_t grapheme_count(P text) noexcept;                            // (4)
}
```

Returns the number of graphemes of the text by [UAX #29](https://www.unicode.org/reports/tr29/): the characters a
reader would count ([graphemes](graphemes.md)), where `size()` is bytes and `rune_count()` code points. What to count
when a limit is a number of characters.

1. Of a slice of UTF-8 bytes.
2. Of a string.
3. Of an array of `char`, a literal among them, read where it lies up to its first NUL or its end, never past it.
4. Of a C text, `const char*` or `char*`, read where it lies up to its NUL.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8; an invalid byte is one code point, `U+FFFD` |

## Return value

The number of graphemes; 0 for an empty text.

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
    string s = "e\u0301\U0001F1F5\U0001F1F1";  // e with a combining acute, then a Polish flag
    println("{} bytes, {} code points, {} characters", s.size(), s.rune_count(),
            txt::grapheme_count(s));

    string text = "Zażółć gęślą jaźń — a potem 漢字 i 🇵🇱 na koniec.";
    println("{} bytes, {} code points, {} characters, {} columns", text.size(), text.rune_count(),
            txt::grapheme_count(text), txt::columns(text));
    println("{} words, {} sentence", txt::words(text).count(), txt::sentences(text).count());
}
```

Output:

```text
11 bytes, 4 code points, 2 characters
67 bytes, 46 code points, 45 characters, 48 columns
10 words, 1 sentence
```

## See also

- [graphemes](graphemes.md): the graphemes themselves
- [columns](columns.md): the cells a text takes on a terminal
- [txt](README.md)

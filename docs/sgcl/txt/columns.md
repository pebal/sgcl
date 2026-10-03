[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::columns

```cpp
#include "sgcl/txt/properties.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr /* unspecified */ columns {};   // called as columns(c) or columns(text)

    constexpr size_t operator()(char32_t c) const noexcept;               // (1)
    size_t operator()(const slice<const char>& text) const noexcept;      // (2)
    size_t operator()(const string& text) const noexcept;                 // (3)
    template<size_t N>
    constexpr size_t operator()(const char (&text)[N]) const noexcept;    // (4)
    template<class P>
    requires std::same_as<P, const char*> || std::same_as<P, char*>
    constexpr size_t operator()(P text) const noexcept;                   // (5)
    size_t operator()(std::string_view) const = delete;                   // (6)
    template<class T>
    requires (!std::same_as<std::remove_cvref_t<T>, char32_t>
              && !std::convertible_to<T, slice<const char>>
              && !std::convertible_to<T, const string&>
              && !std::convertible_to<T, const char*>)
    size_t operator()(T) const = delete;                                  // (7)
}
```

An object called as a function, `txt::columns(c)` or `txt::columns(text)`: the cells a code point or a text
takes on a terminal, by East_Asian_Width. W and F take two cells; a control, a mark that is not a spacing one and a
formatting code point none; the rest one. Below U+0300 only the controls take none, so the soft hyphen takes one; class A (ambiguous — the Greek and Cyrillic letters the East Asian fonts draw wide) is one, as it is
outside a CJK locale. This is the width of a monospaced cell, for a terminal and for aligning columns, and what
`wrap` and `truncate` of the segmentation count; it is not the width of a glyph, and a proportional font is measured
by the one that draws it.

1. The cells of the code point `c`: 0, 1 or 2.
2. The sum over the code points of a text slice.
3. The sum over the code points of a [string](../core/string/README.md).
4. The sum over a literal or another array of `char`, up to its first NUL or its end, whichever comes first: a
   literal does not count its terminating zero.
5. The sum over a C text, up to its NUL; a null pointer is the empty text, and `nullptr` itself does not compile.
6. A `std::string_view` is refused rather than left ambiguous between (2) and (3): the module's texts are the
   library's, which hold what they point at.
7. Every other argument is refused: a `char` (a byte of UTF-8), an `int` (a multi-character literal), the other
   character types.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |
| `text` | the text, UTF-8 |

## Return value

The number of cells.

## Complexity

- (1) Constant: none below U+0300, otherwise up to three lookups, each two reads of a table below U+10000, a binary search over sorted ranges above.
- (2–5) Linear in the length of the text.

## Exceptions

None.

## Notes

The first column of the example is as wide on a terminal as it is in the output above.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

// A table whose first column is text in any script, aligned by what it takes on the terminal
// rather than by how many bytes or code points it is
int main() {
    vector<string> words = {"żółw", "漢字", "Ελλάδα", "𝟛 emoji 😀", "naïve"};
    size_t width = 0;
    for (auto& w : words) {
        width = std::max(width, txt::columns(w));
    }
    for (auto& w : words) {
        string pad(width - txt::columns(w), ' ');
        print("{}{} | {} bytes, {} code points, {} letters", w, pad, w.size(), w.rune_count(),
              w.runes().count_of(txt::is_alpha));
        if (w.runes().exists(txt::is_emoji)) {
            print(", an emoji");
        }
        println();
    }
}
```

Output:

```text
żółw       | 7 bytes, 4 code points, 4 letters
漢字       | 6 bytes, 2 code points, 2 letters
Ελλάδα     | 12 bytes, 6 code points, 6 letters
𝟛 emoji 😀 | 15 bytes, 9 code points, 5 letters, an emoji
naïve      | 6 bytes, 5 code points, 5 letters
```

## See also

- [wrap](wrap.md), [truncate](truncate.md): the segmentation, which counts it
- [format](format.md): the width of a field, which counts grapheme clusters instead
- [sgcl::txt](README.md)

[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::wrap

```cpp
#include "sgcl/txt/segment.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    vector<slice<const char>> wrap(const slice<const char>& text, size_t width) noexcept;    // (1)
    vector<slice<const char>> wrap(const string& text, size_t width) noexcept;               // (2)
    template<size_t N>
    vector<slice<const char>> wrap(const char (&text)[N], size_t width);                     // (3)
    template<class P>
    requires std::same_as<P, const char*> || std::same_as<P, char*>
    vector<slice<const char>> wrap(P text, size_t width);                                    // (4)
}
```

Returns the text laid into lines no wider than `width` **columns** ([columns](columns.md): terminal cells, not bytes
and not code points), cut only where [UAX #14](https://www.unicode.org/reports/tr14/) allows a line to be broken
([line_breaks](line_breaks.md)) and at the hard breaks the text already has. What a monospaced renderer and a table of
columns need.

A line is a slice of the text with its trailing spaces dropped, so nothing is copied. A piece wider than the limit on
its own takes a line of its own and overflows it, because the alternative is cutting a word in half.

1. Of a slice of UTF-8 bytes; the lines hold the slice's object.
2. Of a string; the lines hold the string's object.
3. Of an array of `char`, a literal among them, read up to its first NUL or its end and copied into a string the lines
   hold.
4. Of a C text, `const char*` or `char*`, read up to its NUL and copied likewise.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `width` | the most columns a line may take |

## Return value

The lines, in order, each a slice of the text without its trailing spaces and its line break.

## Complexity

Linear in the length of the text.

## Exceptions

- (1–2) None.
- (3–4) `length_error` when the text is longer than the [max_size()](../core/string/max_size.md) of a string.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string text = "Zażółć gęślą jaźń — a potem 漢字 i 🇵🇱 na koniec.";
    for (auto line : txt::wrap(text, 24)) {
        print("|{}", line);
        for (int i : range(24 - txt::columns(line))) {
            print(" ");
        }
        println("|");
    }
}
```

Output:

```text
|Zażółć gęślą jaźń — a   |
|potem 漢字 i 🇵🇱 na      |
|koniec.                 |
```

## See also

- [line_breaks](line_breaks.md): where a line may be broken
- [truncate](truncate.md): a text cut to a width
- [columns](columns.md): the cells a text takes
- [txt](README.md)

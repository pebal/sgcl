[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::truncate

```cpp
#include "sgcl/txt/segment.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    /*(1)*/ string truncate(const string& text, size_t width, const string& ellipsis);
    /*(2)*/ string truncate(const string& text, size_t width);
}
```

Returns the text cut to fit `width` **columns** ([columns](columns.md): terminal cells), with the ellipsis counted
inside that number and the cut made at a grapheme boundary ([graphemes](graphemes.md)): never in the middle of a
character, however many code points it is. A text that already fits comes back as the same object.

1. With the ellipsis given; a limit too small for the ellipsis alone gives the widest prefix of the ellipsis that
   fits.
2. With `"…"`, one column.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `width` | the most columns the result may take |
| `ellipsis` | what stands for the part cut off |

## Return value

The text, the same object, when it fits in `width` columns; otherwise its longest start of whole graphemes that fits
with the ellipsis, and the ellipsis.

## Complexity

Linear in the length of the text.

## Exceptions

`length_error` when the result would pass the [max_size()](../core/string/max_size.md) of a string.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string text = "Zażółć gęślą jaźń — a potem 漢字 i 🇵🇱 na koniec.";
    println("{}", txt::truncate(text, 20));
    println("{}", txt::truncate(text, 20, " [...]"));
    println("{}", txt::truncate("漢字漢字", 5));  // an ideograph takes two columns

    string fits = "krótki";
    println("{}", txt::truncate(fits, 20).object() == fits.object());
}
```

Output:

```text
Zażółć gęślą jaźń —…
Zażółć gęślą j [...]
漢字…
true
```

## See also

- [wrap](wrap.md): a text laid into lines
- [columns](columns.md): the cells a text takes
- [txt](README.md)

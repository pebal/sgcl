[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::to_upper_full

```cpp
#include "sgcl/txt/case.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    string to_upper_full(const string& text, locale where = {});
}
```

Returns `text` in upper case by Unicode's full case mapping: a letter may become two or three — `"straße"` is
`"STRASSE"`, `"ﬁ"` is `"FI"`, and `"ΐ"` is three code points — and the language `where` tells the ones that spell
an `i` differently: in Turkish and Azerbaijani an `i` keeps its dot, `İ`. A combining dot above that follows an `i` or
a `j` is dropped in Lithuanian (After_Soft_Dotted). `string::to_lower()` and `string::to_upper()` of core map one code point to one, which is right for most text and wrong for `ß`; the name says `_full` because this is what the standard calls the full mapping.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `where` | the language; the root locale by default |

## Return value

The text in upper case; when no letter changes, a text equal to `text`, and `text` itself, the same object, when it
is all ASCII and the locale is the root.

## Complexity

Linear in the length of the text. A text all ASCII, with the root locale, is mapped byte by byte.

## Exceptions

`length_error` when the result would pass a string's `max_size()`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string german = "die straße";
    println("{} | {}", txt::to_upper_full(german), german.to_upper());
    println("ΐ is {} code points in upper case", txt::to_upper_full("ΐ").rune_count());
    println("{}", txt::to_upper_full("istanbul", txt::locale::turkish()));
}
```

Output:

```text
DIE STRASSE | DIE STRAßE
ΐ is 3 code points in upper case
İSTANBUL
```

## See also

- [to_lower_full](to_lower_full.md), [to_title](to_title.md)
- [string::to_upper](../core/string/to_upper.md): one code point to one
- [locale](locale/README.md)
- [sgcl::txt](README.md)

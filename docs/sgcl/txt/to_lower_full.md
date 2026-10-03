[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::to_lower_full

```cpp
#include "sgcl/txt/case.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    string to_lower_full(const string& text, locale where = {});
}
```

Returns `text` in lower case by Unicode's full case mapping: a letter may become more than one code point, a
Greek capital sigma at the end of a word becomes `ς` and anywhere else `σ` (Final_Sigma: a cased letter before it,
none after, marks and apostrophes aside), and the language `where` tells the three that spell an `i` differently —
in Turkish and Azerbaijani an `I` lowers to `ı` and an `I` with a combining dot above to `i` (After_I, Before_Dot), in
Lithuanian an `I` under an accent keeps its dot (More_Above). `string::to_lower()` and `string::to_upper()` of core map one code point to one, which is right for most text and wrong for `ß`; the name says `_full` because this is what the standard calls the full mapping.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `where` | the language; the root locale by default |

## Return value

The text in lower case; when no letter changes, a text equal to `text`, and `text` itself, the same object, when it
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
    string greek = "ΟΔΟΣ ΚΑΙ ΣΠΙΤΙ";
    println("{}", txt::to_lower_full(greek));
    println("{}", greek.to_lower());
    auto tr = txt::locale::turkish();
    println("{} {}", txt::to_lower_full("ISTANBUL", tr), txt::to_lower_full("ISTANBUL"));
}
```

Output:

```text
οδος και σπιτι
οδοσ και σπιτι
ıstanbul istanbul
```

## See also

- [to_upper_full](to_upper_full.md), [to_title](to_title.md), [fold_case](fold_case.md)
- [string::to_lower](../core/string/to_lower.md): one code point to one
- [locale](locale.md)
- [sgcl::txt](README.md)

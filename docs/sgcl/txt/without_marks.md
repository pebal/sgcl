[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::without_marks

```cpp
#include "sgcl/txt/normalize.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    string without_marks(const string& text);
}
```

Returns the text with its accents taken off: decomposed canonically, the nonspacing marks (`Mn`) dropped, composed
again. `"café"` becomes `"cafe"`, `"Grüße"` becomes `"Gruße"` (the umlaut goes and the `ß` stays), `"ἄνθρωπος"`
loses its breathing and its accent, `"Ångström"` becomes `"Angstrom"`. A text with nothing to take off comes back as
the same object; one all ASCII is not looked at past that.

The spacing marks (`Mc`) and the enclosing ones (`Me`) stay: an Indic vowel sign is spelling and not an accent, and
dropping it would take the vowel out of the syllable rather than the accent off a letter.

What it does **not** do is worth more than what it does, because the name of this operation promises more than any
implementation of it can give:

- **It is not a transliteration.** A letter whose mark is part of the letter and not a mark at all comes through
  untouched, because it has no canonical decomposition to take apart: `Ł`, `ø`, `đ`, `ħ`, `ı` and `ß` are letters of
  their alphabets. `"Łódź"` becomes `"Łodz"` with its `Ł` still an `Ł`, and `"żółć"` becomes `"zołc"`. Turning those
  into Latin letters is a mapping a language chooses, not one Unicode holds.
- **It is not a slug.** It does not lower the case, it does not touch the spaces or the punctuation, and it does not
  drop what is not a letter. It is composed with [to_lower_full](to_lower_full.md) and with whatever rule the URLs
  want.
- **It is not a way of comparing names.** Two words that differ only in an accent are different words in most
  languages that write accents, and a comparison that ignores them will say Polish `"łasa"` and `"lasa"` are one
  word. [fold_case](fold_case.md), [nfkc_casefold](nfkc_casefold.md) and the [collator](collator.md) at its first
  strength are what compare text.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8; an invalid byte is read as `U+FFFD` |

## Return value

The text without its nonspacing marks, in NFC; `text` itself, the same object, when nothing was taken off.

## Complexity

Linear in the length of the text.

## Exceptions

`length_error` when the result would pass the [max_size()](../core/string/max_size.md) of a string.

## Notes

It is a function of the normalization and not of the [identifiers](is_identifier.md) because it needs no table of its
own: it is the normalization's walk with one filter in it. Its cost is measured with the identifiers'
([Benchmarks: Identifiers](benchmarks.md#identifiers)).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto s : {"café", "Grüße", "Ångström", "Łódź", "żółć"}) {
        print("{} -> {}   ", s, txt::without_marks(s));
    }
    println();

    string plain = "Ala ma kota";
    println("{}", txt::without_marks(plain).object() == plain.object());
}
```

Output:

```text
café -> cafe   Grüße -> Gruße   Ångström -> Angstrom   Łódź -> Łodz   żółć -> zołc   
true
```

## See also

- [normalize](normalize.md): the forms the walk is made of
- [fold_case](fold_case.md), [nfkc_casefold](nfkc_casefold.md): what compares text
- [txt](README.md)

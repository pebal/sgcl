[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::nfkc_casefold

```cpp
#include "sgcl/txt/identifier.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    string nfkc_casefold(const string& text);
}
```

Returns the text in NFKC_Casefold ([UAX #15](https://www.unicode.org/reports/tr15/)), the form a name is compared in
when the comparison must not care how the name was written, what case it was written in, or whether a compatibility
form was used: `"ＦＵＬＬ"` and `"full"` fold to one text, `"ﬁle"` and `"file"` fold to one, and so do `"é"` written
as one code point and as two. It is what [UTS #46](https://www.unicode.org/reports/tr46/) folds a domain label to,
what [UAX #31](https://www.unicode.org/reports/tr31/) recommends a compiler compare identifiers by, and what a
registry folds a name to before it asks whether the name is taken. It also **drops the default ignorable code
points**, which is the half that is not a folding: a soft hyphen hidden inside `"pay­pal"` is gone.

It is not for showing to anybody. The mapping cannot be undone, it loses the case, and it loses the difference
between a ligature and the letters in it. Fold to compare, keep the original to print.

A text already in the form comes back as **the same object**: over ASCII the whole mapping is the one bit of the
case, and otherwise two properties answer it without folding anything ([is_nfkc_casefolded](is_nfkc_casefolded.md)).

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8; an invalid byte is read as `U+FFFD` |

## Return value

The folded text; `text` itself, the same object, when it is in the form already.

## Complexity

Linear in the length of the text.

## Exceptions

`length_error` when the result would pass the [max_size()](../core/string/max_size.md) of a string.

## Notes

- **The mapping is not a table here.** `DerivedNormalizationProps.txt` would be 91.4 KB of prototypes, and every one
  of them is `NFC(fold(NFKD(c)))` with the default ignorable code points dropped, for all 1 114 112 code points,
  which the generator asserts before it writes anything. So the module works it out from the compatibility
  decompositions and the full case folding it carries anyway, and keeps 2.9 KB of properties instead of 91.4 KB of
  mapping.
- The order matters and is not the order one would guess. The text is **not** decomposed as a whole first:
  `U+0345 COMBINING GREEK YPOGEGRAMMENI` has combining class 240 and folds to `U+03B9 GREEK SMALL LETTER IOTA`, which
  has none, so decomposing the whole text would sort it past the marks that follow it and the fold would then leave
  it there. Each code point is taken apart on its own, folded, and only then is the whole put in canonical order and
  composed, which is where the standard draws the same line, its mapping being per code point with one NFC at the
  end.
- The oracle is the NFKC_CF mapping of `DerivedNormalizationProps.txt`, **entry by entry**, all 10 554 of them, and
  the identity of the other 1 103 558.
- Folding does **not** catch a name written in another script: the Cyrillic `а` is a different letter and not a
  different writing of the Latin one. That is what [is_confusable](is_confusable.md) and the
  [restriction level](restriction_level_of.md) are for, and it is why a registry needs all three.
- What the fold costs against [fold_case](fold_case.md), and the other functions of the identifiers, is on
  [Benchmarks: Identifiers](benchmarks.md#identifiers).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto s : {"ＦＵＬＬ", "ﬁle", "Straße", "①②③", "pay\u00ADpal"}) {
        print("{} -> {}   ", s, txt::nfkc_casefold(s));
    }
    println();

    string wanted = "раypal";  // the а and the р are Cyrillic
    println("{}", txt::nfkc_casefold(wanted) == txt::nfkc_casefold("paypal"));
}
```

Output:

```text
ＦＵＬＬ -> full   ﬁle -> file   Straße -> strasse   ①②③ -> 123   pay­pal -> paypal   
false
```

## See also

- [is_nfkc_casefolded](is_nfkc_casefolded.md): whether a text is in the form already
- [fold_case](fold_case.md): the case folding alone
- [normalize](normalize.md): the normalization forms alone
- [txt](README.md)

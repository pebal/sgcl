[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::normalize

```cpp
#include "sgcl/txt/normalize.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    template<class Form>
    string normalize(const string& text, Form form);
}
```

Returns the text in one of the four normalization forms of [UAX #15](https://www.unicode.org/reports/tr15/):
`txt::nfc`, `txt::nfd`, `txt::nfkc` or `txt::nfkd` ([the forms](nfc_t.md)).

The same text can be written in more than one way. `"é"` is one code point or two, `"각"` is one or three, and
`"Å"` is three different code points that a reader would not tell apart. Normalization puts a text into one form,
so that two texts a reader calls the same compare the same, which is what a search, a key of a map and a file name
need.

A text that is already in the form comes back as **the same object**. The strings of the library are immutable and
shared by copying, so the common case, text that arrives normalized, which on the web is nearly all of it,
allocates nothing and copies nothing. Whether it is in the form is answered by the quick check properties of the
UCD, which settle most texts without looking at a single decomposition; where they say "maybe", the text is
normalized and compared, and a result equal to the text is the text's own object as well.

An invalid byte of UTF-8 is in no form: a text with one is rebuilt, with `U+FFFD` in the byte's place, whatever
the rest of the text is. `U+FFFD` written as itself is a code point like any other.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `form` | `txt::nfc`, `txt::nfd`, `txt::nfkc` or `txt::nfkd` |

## Return value

The text in the form; `text` itself, the same object, when it is in the form already.

## Complexity

Linear in the length of the text. The marks of one character are put in canonical order by an insertion sort,
quadratic in their number, which is a handful.

## Exceptions

`length_error` when the result would pass the [max_size()](../core/string/max_size.md) of a string.

## Notes

- The oracle is `NormalizationTest.txt`, the UCD's conformance file: **19 965 lines**, each of them five ways of
  writing one text, with every invariant the standard states beside them, and the other half of the test, that
  every one of the **1 112 064 code points** part one does not name is its own normalization in all four forms.
  Both pass whole.
- The tables are 68 KB: the compatibility decompositions 36.2, the canonical ones 19.8, the primary composites 5.8,
  the quick check properties 3.7 and the canonical combining class 2.9. The Hangul syllables are in none of them:
  eleven thousand of them decompose and compose by arithmetic. The composition exclusions are not a list either: a
  canonical pair is a primary composite when NFC puts it back together, which the generator asks rather than reads.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string syllable = "각";
    string jamo = txt::normalize(syllable, txt::nfd);
    println("{}: {} code point, {} decomposed, back: {}", syllable, syllable.rune_count(),
            jamo.rune_count(), txt::normalize(jamo, txt::nfc) == syllable);

    for (auto s : {"ﬁ", "①", "Ａ", "½"}) {
        print("{} -> {}   ", s, txt::normalize(s, txt::nfkc));
    }
    println();

    string plain = "Ala ma kota";
    println("{}", txt::normalize(plain, txt::nfc).object() == plain.object());
}
```

Output:

```text
각: 1 code point, 3 decomposed, back: true
ﬁ -> fi   ① -> 1   Ａ -> A   ½ -> 1⁄2   
true
```

## See also

- [the forms](nfc_t.md): `nfc`, `nfd`, `nfkc`, `nfkd`
- [is_normalized](is_normalized.md): whether a text is in a form already
- [equal_normalized](equal_normalized.md): two texts compared through their normalization
- [nfkc_casefold](nfkc_casefold.md): the form names are compared in
- [txt](README.md)

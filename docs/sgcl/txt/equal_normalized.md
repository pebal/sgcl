[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::equal_normalized

```cpp
#include "sgcl/txt/normalize.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    bool equal_normalized(const string& a, const string& b) noexcept;
}
```

Checks whether two texts are the same text, however written: canonical equivalence, both texts compared in NFD.
`"é"` of one code point and `"é"` of two are equal; `"ﬁ"` and `"fi"` are not, a compatibility form being another
text (compare their [nfkc](nfc_t.md) or their [nfkc_casefold](nfkc_casefold.md) for that). Two texts equal as bytes
are equal without being decomposed. What a search and a key of a map want.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the texts, UTF-8 |

## Return value

`true` when the two texts have the same canonical decomposition.

## Complexity

Linear in the lengths of the two texts.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    // every letter that has a mark, written with it
    string typed = "Zaz\u0307o\u0301\u0142c\u0301 ge\u0328s\u0301la\u0328 jaz\u0301n\u0301";
    string pasted = "Zażółć gęślą jaźń";  // the precomposed form
    println("{} vs {} bytes, {} vs {} code points, {} vs {} characters", typed.size(),
            pasted.size(), typed.rune_count(), pasted.rune_count(), txt::grapheme_count(typed),
            txt::grapheme_count(pasted));
    println("equal as bytes: {}, as text: {}", typed == pasted,
            txt::equal_normalized(typed, pasted));
    println("{}", txt::equal_normalized("ﬁ", "fi"));
}
```

Output:

```text
34 vs 26 bytes, 25 vs 17 code points, 17 vs 17 characters
equal as bytes: false, as text: true
false
```

## See also

- [compare_normalized](compare_normalized.md): the order blind to the writing
- [hash_normalized](hash_normalized.md): the hash this equality agrees with
- [normalize](normalize.md): the text in a form
- [txt](README.md)

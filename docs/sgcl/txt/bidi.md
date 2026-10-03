[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::bidi

```cpp
#include "sgcl/txt/bidi.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class bidi : uint8_t {
        l, r, al, en, es, et, an, cs, nsm, bn, b, s, ws, on,
        lre, lro, rle, rlo, pdf, lri, rli, fsi, pdi,
    };
}
```

The bidirectional class of a code point, the `Bidi_Class` property of
[UAX #9](https://www.unicode.org/reports/tr9/): what the bidirectional algorithm knows about a character before it
looks at anything around it. [bidi_class_of](bidi_class_of.md) answers it for one code point; the algorithm that
reads it runs in [bidi_runs](bidi_runs/README.md), [levels](levels.md), [visual_order](visual_order.md),
[paragraph_direction](paragraph_direction.md) and [mirrored](mirrored.md), and the bidirectional rule of
[IDNA](idna/README.md) reads it too.

| Value | Description |
|---|---|
| `l` | Left_To_Right: a strong letter of a script written left to right, Latin, Greek, Cyrillic, Han |
| `r` | Right_To_Left: a strong letter of Hebrew and the other scripts written right to left |
| `al` | Arabic_Letter: a strong letter of Arabic, Syriac, Thaana |
| `en` | European_Number: the digits `0`–`9` and the other European digits |
| `es` | European_Separator: `+` and `-` |
| `et` | European_Terminator: a currency or a degree sign, `%`, `#` |
| `an` | Arabic_Number: the Arabic-Indic digits and the Arabic separators of numbers |
| `cs` | Common_Separator: `,`, `.`, `/`, `:` and the no-break space |
| `nsm` | Nonspacing_Mark: a mark, which takes the class of the character it stands on |
| `bn` | Boundary_Neutral: the controls and the format characters the algorithm passes over |
| `b` | Paragraph_Separator: a line feed, a carriage return, U+2029 |
| `s` | Segment_Separator: a tab |
| `ws` | White_Space: a space |
| `on` | Other_Neutral: the other punctuation and symbols |
| `lre` | Left_To_Right_Embedding, U+202A |
| `lro` | Left_To_Right_Override, U+202D |
| `rle` | Right_To_Left_Embedding, U+202B |
| `rlo` | Right_To_Left_Override, U+202E |
| `pdf` | Pop_Directional_Format, U+202C |
| `lri` | Left_To_Right_Isolate, U+2066 |
| `rli` | Right_To_Left_Isolate, U+2067 |
| `fsi` | First_Strong_Isolate, U+2068 |
| `pdi` | Pop_Directional_Isolate, U+2069 |

## Rules

- The classes come from `DerivedBidiClass.txt` rather than from the assigned code points alone, because the file
  gives a class to the unassigned ones too: the ranges of Hebrew and Arabic run right to left before anything is
  put in them, and a text with a code point from a future version must still lay out sensibly.
- The tables of the bidirectional algorithm are 13.4 KB: the class of every code point 10.2, the 128 bracket pairs
  1.0 and the mirroring 2.1 ([mirrored_of](mirrored_of.md)).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string line = "Nazwa: שלום 123";
    int strong_rtl = 0;
    for (char32_t c : line.runes()) {
        auto k = txt::bidi_class_of(c);
        strong_rtl += k == txt::bidi::r || k == txt::bidi::al;
    }
    println("{} right to left, {}", strong_rtl, txt::bidi_class_of(U'7') == txt::bidi::en);
}
```

Output:

```text
4 right to left, true
```

## See also

- [bidi_class_of](bidi_class_of.md): the class of a code point
- [bidi_runs](bidi_runs/README.md): the algorithm run over a text
- [txt](README.md)

[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::category

```cpp
#include "sgcl/txt/properties.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class category : uint8_t {
        unassigned, uppercase_letter, lowercase_letter, titlecase_letter, modifier_letter,
        other_letter, nonspacing_mark, spacing_mark, enclosing_mark, decimal_number, letter_number,
        other_number, connector_punctuation, dash_punctuation, open_punctuation, close_punctuation,
        initial_punctuation, final_punctuation, other_punctuation, math_symbol, currency_symbol,
        modifier_symbol, other_symbol, space_separator, line_separator, paragraph_separator,
        control, format, surrogate, private_use
    };
}
```

The general category of a code point, as [category_of](category_of.md) answers it, spelled out as ICU spells it,
the abbreviation of the UCD beside each. The order is the UCD's, so a run of categories is a range: `is_alpha` is one
comparison of a pair, not five.

| Value | Description |
|---|---|
| `unassigned` | Cn: no category: unassigned, two thirds of the code space; zero, so it needs no range of its own in the table |
| `uppercase_letter` | Lu: an upper case letter |
| `lowercase_letter` | Ll: a lower case letter |
| `titlecase_letter` | Lt: a title case letter, a digraph such as `ǅ` |
| `modifier_letter` | Lm: a modifier letter |
| `other_letter` | Lo: another letter, one with no case: Hebrew, Han, Thai... |
| `nonspacing_mark` | Mn: a nonspacing mark, the combining acute |
| `spacing_mark` | Mc: a spacing mark |
| `enclosing_mark` | Me: an enclosing mark |
| `decimal_number` | Nd: a decimal digit |
| `letter_number` | Nl: a letter number, a Roman numeral |
| `other_number` | No: another number: a superscript, a fraction |
| `connector_punctuation` | Pc: a connector, `_` |
| `dash_punctuation` | Pd: a dash |
| `open_punctuation` | Ps: an opening bracket |
| `close_punctuation` | Pe: a closing bracket |
| `initial_punctuation` | Pi: an opening quotation mark |
| `final_punctuation` | Pf: a closing quotation mark |
| `other_punctuation` | Po: other punctuation: `!`, `?`, `.` |
| `math_symbol` | Sm: a mathematical symbol |
| `currency_symbol` | Sc: a currency symbol |
| `modifier_symbol` | Sk: a modifier symbol |
| `other_symbol` | So: another symbol |
| `space_separator` | Zs: a space separator, the space itself among them |
| `line_separator` | Zl: the line separator |
| `paragraph_separator` | Zp: the paragraph separator |
| `control` | Cc: a control |
| `format` | Cf: a formatting code point |
| `surrogate` | Cs: a surrogate |
| `private_use` | Co: a code point of private use |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string text = "«Ok», 3€";
    for (char32_t c : text.runes()) {
        auto k = txt::category_of(c);
        println("U+{:04X} {} {}", uint32_t(c), int(k), k == txt::category::initial_punctuation);
    }
}
```

Output:

```text
U+00AB 16 true
U+004F 1 false
U+006B 2 false
U+00BB 17 false
U+002C 18 false
U+0020 23 false
U+0033 9 false
U+20AC 20 false
```

## See also

- [category_of](category_of.md)
- [sgcl::txt](README.md)

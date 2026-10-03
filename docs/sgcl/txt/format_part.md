[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::format_part

```cpp
#include "sgcl/txt/format.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct format_part {
        static constexpr uint8_t no_value = 255;

        uint16_t at = 0;
        uint16_t size = 0;
        uint16_t width = 0;
        int16_t precision = -1;
        uint16_t nested_at = 0;
        uint8_t nested_size = 0;
        uint8_t which = no_value;
        uint8_t flags = 0;
        char fill = ' ';
        char align = 0;
        char sign = 0;
        char type = 0;
    };
}
```

`sgcl::txt::format_part` is one step of a pattern read where the program is compiled
([format_pattern::parts](format_pattern/parts.md)): a run of the literal text of the pattern, and the field that
follows it with its [specification](format_spec.md) already made out. A call of [format](format.md) walks the steps
rather than the characters of the pattern, so a literal pattern is not read again where the program runs.

The fields of a specification are kept in eighteen bytes rather than the thirty-two of a `format_spec`, because a
pattern is built afresh at every call, and what that costs is the bytes it takes. The text handed on to what a value
holds (after a second colon) is kept as where it stands in the pattern, not as characters.

## Rules

- A plain struct of numbers and characters: it lives anywhere. The steps of a pattern are made by its `consteval`
  constructor and do not change.
- The positions are bytes of the text of the pattern, [view](format_pattern/view.md).

## Member objects

| Field | Description |
|---|---|
| `at` | where the run of literal text starts in the pattern |
| `size` | the bytes of the run; `0` when the field follows another directly |
| `width` | the width of the field; with flag 8, the number of the value that gives it |
| `precision` | the precision, `-1` for none; with flag 16, the number of the value that gives it |
| `nested_at` | with flag 4, where the specification of what the value holds starts in the pattern |
| `nested_size` | with flag 4, its length |
| `which` | the number of the value written after the run; `no_value` for a step of literal text alone |
| `flags` | `1` the `#` of the field, `2` its `0`, `4` a specification for what the value holds, `8` a width and `16` a precision a value gives |
| `fill`, `align`, `sign`, `type` | as in a [format_spec](format_spec.md); a pattern whose fill is a code point above ASCII keeps no steps and is read where it runs |

| Constant | Value | Description |
|---|---|---|
| `no_value` | `255` | the `which` of a step with no field, the literal text after the last field, `static constexpr uint8_t` |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    constexpr txt::format_pattern<int, double> pattern("id {:>4}, {:.2f} s");
    for (size_t i : range(pattern.count())) {
        const txt::format_part& p = pattern.parts()[i];
        auto text = pattern.view().substr(p.at, p.size);
        if (p.which == txt::format_part::no_value) {
            println("[{}]", text);
        } else {
            println("[{}] value {} width {} precision {}", text, p.which, p.width, p.precision);
        }
    }
}
```

Output:

```text
[id ] value 0 width 4 precision -1
[, ] value 1 width 0 precision 2
[ s]
```

## See also

- [format_pattern](format_pattern/README.md): the pattern the steps are read from
- [format_spec](format_spec.md): the specification of one field, as a value writes itself by it

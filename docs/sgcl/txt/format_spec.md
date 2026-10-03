[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::format_spec

```cpp
#include "sgcl/txt/format.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct format_spec;
}
```

How a value is to be written: one field's [specification](format.md#the-specification) as it was read,
`[[fill]align][sign][#][0][width][.precision][type]`, each part in the member of its name. It is what a
`format_value` of a type of one's own and the `write` of a [formatter](formatter.md) are handed, already checked
against what the type takes, and what [write_padded](write_padded.md) reads the fill, the alignment, the width and
the precision from. `std::format` keeps the same parts inside its `std::formatter`, where a program reaches them by
parsing the specification itself.

## Rules

- A plain aggregate of characters and numbers: it lives anywhere and is copied freely.
- A part that was not written keeps its default, so a formatter tells "none given" from every value written.

## Member objects

| Member | Description |
|---|---|
| `char32_t fill` | the code point a field is padded with, written as UTF-8 once for every column of padding; `U' '` by default |
| `char align` | `'<'`, `'>'` or `'^'`; `0`, none given, by default (a number goes to the right, text to the left) |
| `char sign` | `'+'`, `'-'` or `' '`; `0`, none given, by default |
| `bool alternate` | `#`: `0x` and `0b` before a number, a point that stays; `false` by default |
| `bool zero` | `0`: a number padded with zeros after its sign; `false` by default |
| `unsigned width` | the width of the field, in columns for text; `0` by default. One a value gives (`{:>{}}`) is here as that value, held to 0–65535 |
| `int precision` | the precision; `-1`, none given, by default. One a value gives (`{:.{}}`) is here as that value, held to 32767, a negative one `-1` |
| `char type` | the type: `'d'`, `'x'`, `'f'`, `'s'`, `'c'`, `'p'`, `'?'`, …; `0`, none given, by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

namespace probe {
    struct field {};

    void format_value(txt::format_sink& out, field, const txt::format_spec& spec) {
        string text = txt::format("fill {:?} align {:?} sign {:?} # {} 0 {} "
                                  "width {} precision {} type {:?}",
                                  spec.fill, spec.align ? spec.align : '-',
                                  spec.sign ? spec.sign : '-', spec.alternate, spec.zero,
                                  spec.width, spec.precision, spec.type ? spec.type : '-');
        out.put(text.view());
    }
}

int main() {
    println("{}", txt::format("{}", probe::field{}));
    println("{}", txt::format("{:*^+#012.3x}", probe::field{}));
    return 0;
}
```

Output:

```text
fill ' ' align '-' sign '-' # false 0 false width 0 precision -1 type '-'
fill '*' align '^' sign '+' # true 0 true width 12 precision 3 type 'x'
```

## See also

- [format](format.md#the-specification): what a specification may say
- [format_sink](format_sink.md), [write_padded](write_padded.md): where a value is written, and how a text is padded
- [formatter](formatter.md): what decides which specifications a type takes

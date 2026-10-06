[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::number_format

```cpp
#include "sgcl/txt/number.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class number_format;
}
```

`sgcl::txt::number_format` is a locale's way of writing numbers (LDML Part 3) with a choice of options, resolved
once: its digits, its decimal separator and grouping, percent, scientific notation, the compact forms and amounts of
a currency, from CLDR 46's data for every locale. Where Go's `golang.org/x/text/message` prints numbers in a few
locales and ICU's `NumberFormatter` builds a formatter in a chain of calls, this is one object made from a locale
and an [options](../number_options.md) struct.

A number is written from its decimal digits — an integer exactly, a floating-point number from its shortest digits,
decimal text to 200 significant digits — and rounded half to even unless the options say otherwise. The output is
ICU's: held against ICU 78 over thousands of cases, the differences those of its later data.

## Rules

- A plain value of a few dozen bytes: indices into constant tables, the options, no tracked word. It lives
  anywhere and is copied freely; [format](format.md) is `const` and may be called from many threads at once.
- Nothing fails over a number: a NaN and an infinity have the locale's symbols, a number too long for a stack
  buffer is written in a second pass. Decimal text that is not a number is `nullopt`.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](number_format.md) | constructs the root locale's decimal format, or a locale's with options |

#### Formatting

| Function | Description |
|---|---|
| [format](format.md) | a number as text; decimal text as an `optional` |
| [format_to](format_to.md) | a number into memory the caller lends, nothing allocated |

#### Observers

| Function | Description |
|---|---|
| [where](where.md) | the locale |
| [options](options.md) | the options |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::number_format pl(txt::locale("pl"), {.style = txt::number_style::compact_long});
    for (double x : {1234.0, 2345678.0, 5000000.0}) {
        println("{}", pl.format(x));
    }
}
```

Output:

```text
1,2 tysiąca
2,3 miliona
5 milionów
```

## See also

- [format_number](../format_number.md), [format_currency](../format_currency.md): the one-line forms
- [number_options](../number_options.md), [number_style](../number_style.md)
- [plural_of](../plural_of.md)
- [locale](../locale/README.md)
- [sgcl::txt](../README.md)

[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::number_options

```cpp
#include "sgcl/txt/number.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct number_options {
        number_style style = number_style::decimal;
        int min_integer = 1;
        int min_fraction = -1;
        int max_fraction = -1;
        int min_significant = 0;
        int max_significant = 0;
        bool grouping = true;
        sign_display sign = sign_display::automatic;
        sgcl::rounding mode = sgcl::rounding::half_even;
        txt::currency currency;
        currency_display display = currency_display::symbol;
        bool cash = false;
    };
}
```

`sgcl::txt::number_options` is how a [number_format](number_format/README.md) writes a number, beside the locale:
the style, the digits, the sign, the rounding and, for an amount, the currency. Every field has the locale's or the
style's default, so an aggregate names only what differs: `{.style = txt::number_style::percent}`, `{.max_fraction =
2}`. A value of a few bytes that holds no tracked word.

## Member objects

| Field | Description |
|---|---|
| `style` | a [number_style](number_style.md); `decimal` by default |
| `min_integer` | zeros in front up to this many integer digits; 1 by default |
| `min_fraction` | the least fraction digits, zeros added; -1, the default, is the style's: 0, and a currency's digits |
| `max_fraction` | the most fraction digits, the rest rounded away; -1, the default, is the style's: 3 for decimal and scientific, 0 for percent and per mille, a currency's digits. One given beyond the other moves it along; a thousand at most |
| `min_significant` | with `max_significant`, the least significant digits shown, zeros added: 1.00 for three |
| `max_significant` | significant digits instead of fraction digits, the rest rounded away; 0, the default: fraction digits |
| `grouping` | the locale's separators of thousands, by its sizes and its minimum grouping digits (Polish groups from five digits, 12 345, and a compact form from five too); `true` by default |
| `sign` | a [sign_display](sign_display.md); `automatic` by default |
| `mode` | the shared `sgcl::rounding` of core (`sgcl/core/rounding.h`); `half_even` by default, as CLDR and ICU round. `unnecessary` drops no digit at all |
| `currency` | the [currency](currency/README.md) of a currency style; none, the default, is the locale's region's ([currency::of](currency/of.md)) |
| `display` | a [currency_display](currency_display.md); `symbol` by default |
| `cash` | the currency's cash digits and its rounding step: Swiss francs to 0.05; `false` by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto en = txt::locale("en");
    println("{}", txt::format_number(3.14159, en, {.max_fraction = 2}));
    println("{}", txt::format_number(42, en, {.min_integer = 5, .grouping = false}));
    println("{}", txt::format_number(123456, en, {.max_significant = 2}));
    println("{}",
            txt::format_number(2.5, en, {.max_fraction = 0, .mode = sgcl::rounding::half_up}));
    println("{}", txt::format_number(1.23, txt::locale("de-CH"),
                                     {.style = txt::number_style::currency, .cash = true}));
}
```

Output:

```text
3.14
00042
120,000
3
CHF 1.25
```

## See also

- [number_format](number_format/README.md)
- [format_number](format_number.md)
- [sgcl::txt](README.md)

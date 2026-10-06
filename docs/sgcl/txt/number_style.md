[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::number_style

```cpp
#include "sgcl/txt/number.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class number_style : uint8_t {
        decimal,
        percent,
        permille,
        scientific,
        compact,
        compact_long,
        currency,
        accounting,
        currency_compact,
    };
}
```

How a [number_format](number_format/README.md) writes a number: which of the locale's patterns (CLDR's decimal,
percent, scientific, currency and accounting patterns and its compact forms) it takes.

| Value | Description |
|---|---|
| `decimal` | `1,234.568`: three fraction digits at most |
| `percent` | `12%`: the value times 100, no fraction digits |
| `permille` | `12‰`: the value times 1000, in the percent pattern with the per mille sign |
| `scientific` | `1.235E3`: three fraction digits of the mantissa at most |
| `compact` | `1.2K`, `1,2 tys.`: the short compact forms, rounded to two significant digits under 100 and to an integer from 100 |
| `compact_long` | `1.2 thousand`, `1,2 tysiąca`: the long ones, the word in the number's plural form |
| `currency` | `$1,234.57`: the currency's digits, its symbol where the locale puts it |
| `accounting` | `($1,234.57)`: a negative amount as the locale's accountants write it |
| `currency_compact` | `$1.2K`: the short compact forms of a currency |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto s : {txt::number_style::decimal, txt::number_style::percent,
                   txt::number_style::scientific, txt::number_style::compact,
                   txt::number_style::compact_long, txt::number_style::currency}) {
        println("{}", txt::format_number(1234.5678, txt::locale("en"), {.style = s}));
    }
}
```

Output:

```text
1,234.568
123,457%
1.235E3
1.2K
1.2 thousand
$1,234.57
```

## See also

- [number_options](number_options.md)
- [number_format](number_format/README.md)
- [sgcl::txt](README.md)

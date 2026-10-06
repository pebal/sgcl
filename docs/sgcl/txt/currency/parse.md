[sgcl](../../README.md) › [txt](../README.md) › [currency](README.md)

# sgcl::txt::currency::parse

```cpp
static expected<currency, code_error> parse(const string& code) noexcept;
```

Reads a currency code from outside the program: three ASCII letters in any case, `"pln"` is PLN. A code ISO 4217
does not list is accepted, as ICU accepts it: it has two digits and its code for a symbol.

## Parameters

| Parameter | Description |
|---|---|
| `code` | the text |

## Return value

The currency, or a [code_error](../code_error/README.md) with the byte the reading stopped on.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto code : {"eur", "XYZ", "zl", "P1N"}) {
        auto c = txt::currency::parse(code);
        if (c) {
            println("{} {}", code, c->code());
        } else {
            println("{} {}", code, c.error().message());
        }
    }
}
```

Output:

```text
eur EUR
XYZ XYZ
zl not a currency code: three ASCII letters expected
P1N not a currency code: three ASCII letters expected
```

## See also

- [(constructor)](currency.md)
- [code_error](../code_error/README.md)
- [sgcl::txt::currency](README.md)

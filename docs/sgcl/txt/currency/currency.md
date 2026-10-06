[sgcl](../../README.md) › [txt](../README.md) › [currency](README.md)

# sgcl::txt::currency::currency

```cpp
constexpr currency() noexcept = default;    // (1)
explicit currency(const string& code);      // (2)
```

Constructs a currency.

1. No currency: its [operator bool](operator_bool.md) is `false`, and a [number_format](../number_format/README.md)
   of a currency style then takes the locale's region's.
2. The currency of the code `code`, three ASCII letters in any case, as [parse](parse.md) reads it: a code the
   program itself writes. A code ISO 4217 does not list is a currency still, with two digits and its code for a
   symbol.

## Parameters

| Parameter | Description |
|---|---|
| `code` | the code: `"PLN"`, `"usd"` |

## Complexity

Constant.

## Exceptions

`bad_expected_access<code_error>` with the error of [parse](parse.md) when `code` is not three ASCII letters (2).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::currency pln("pln");
    println("{} {}", pln.code(), bool(txt::currency()));
}
```

Output:

```text
PLN false
```

## See also

- [parse](parse.md)
- [of](of.md)
- [sgcl::txt::currency](README.md)

[sgcl](../../README.md) › [txt](../README.md) › [number_format](README.md)

# sgcl::txt::number_format::number_format

```cpp
number_format() noexcept;                                                          // (1)
explicit number_format(const locale& l, const number_options& o = {}) noexcept;    // (2)
```

Constructs a way of writing numbers, the locale's data looked up once.

1. The root locale's decimal format.
2. The locale `l` with the options `o`: its numbering system's digits and symbols (Latin ones for `-u-nu-latn`),
   the pattern of the style, the currency and its symbol.

## Parameters

| Parameter | Description |
|---|---|
| `l` | the locale |
| `o` | the options, [number_options](../number_options.md) |

## Complexity

Logarithmic in the number of locales.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::number_format pl(txt::locale("pl"));
    for (double x : {1.5, 1234.5, 1234567.0}) {
        println("{}", pl.format(x));
    }
}
```

Output:

```text
1,5
1234,5
1 234 567
```

## See also

- [format](format.md)
- [sgcl::txt::number_format](README.md)

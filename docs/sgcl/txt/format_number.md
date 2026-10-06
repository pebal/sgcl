[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::format_number

```cpp
template<class T>
string format_number(T value, const locale& l = {}, const number_options& o = {});
```

Returns a number as a locale writes it: `number_format(l, o).format(value)`, the one-line form. In Polish
1234567.891 is "1 234 567,891", in Hindi 123456789.5 "12,34,56,789.5", in Egyptian Arabic in Arabic digits; the
options choose the style, the digits, the sign, the rounding and the currency.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the number: any arithmetic type but `bool` (an integer exactly, a floating-point number by its shortest digits) |
| `l` | the locale; the root locale by default |
| `o` | the options, [number_options](number_options.md) |

## Return value

The text.

## Complexity

Linear in the digits written, after a lookup logarithmic in the number of locales.

## Exceptions

None that the program can cause; running out of memory ends it.

## Notes

Takes part only when `T` is an arithmetic type other than `bool`. A program that formats many numbers in one way
keeps a [number_format](number_format/README.md), which resolves the locale once.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}", txt::format_number(1234567.891, txt::locale("pl")));
    println("{}", txt::format_number(123456789.5, txt::locale("hi")));
    println("{}",
            txt::format_number(0.256, txt::locale("de"), {.style = txt::number_style::percent}));
    println("{}", txt::format_number(1234567, txt::locale("pl"),
                                     {.style = txt::number_style::compact_long}));
}
```

Output:

```text
1 234 567,891
12,34,56,789.5
26 %
1,2 miliona
```

## See also

- [number_format](number_format/README.md)
- [format_currency](format_currency.md)
- [sgcl::txt](README.md)

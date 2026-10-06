[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::plural_of

```cpp
template<class T>
plural plural_of(T n, const locale& l) noexcept;                     // (1)
plural plural_of(double n, const locale& l) noexcept;                // (2)
plural plural_of(const string& number, const locale& l) noexcept;    // (3)
```

Returns the cardinal plural category of a number in a locale's language ("1 plik", "2 pliki", "5 plików"), by the
rules of CLDR 46 (`plurals.xml`) — those of the language and region where CLDR has them apart (`pt-PT`), else of the
language, else of root (`other` for everything). A rule asks about the number as written, through the operands of
TR35 §5.1 (the integer digits, the visible fraction digits with and without trailing zeros, a compact exponent), and
of a negative number about its absolute value.

1. An integer of any integral type but `bool`: exact.
2. A double, by its shortest decimal digits: 1.5 has one fraction digit and 1.0 none. A NaN or an infinity is `other`.
3. A number written as decimal text, as it will be shown: `"1.50"` has two fraction digits, `"1.2c6"` (or `"1.2e6"`)
   is a compact 1.2 million, as CLDR writes it. The text is read as far as it is a number: `"2kg"` is 2, an empty
   text 0.

## Parameters

| Parameter | Description |
|---|---|
| `n`, `number` | the number |
| `l` | the locale whose language's rules apply |

## Return value

The category.

## Complexity

Logarithmic in the number of languages; (3) linear in the length of the text.

## Exceptions

None.

## Notes

(1) takes part only when `T` is an integral type other than `bool`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto pl = txt::locale("pl");
    auto en = txt::locale("en");
    println("{} {} {}", int(txt::plural_of(5, pl)), int(txt::plural_of(1.5, pl)),
            int(txt::plural_of("1.0", en)));
    println("{}", txt::plural_of(1e6, txt::locale("fr")) == txt::plural::many);
}
```

Output:

```text
4 5 5
true
```

## See also

- [plural](plural.md)
- [ordinal_of](ordinal_of.md)
- [sgcl::txt](README.md)

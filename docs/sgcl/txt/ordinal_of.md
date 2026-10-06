[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::ordinal_of

```cpp
template<class T>
plural ordinal_of(T n, const locale& l) noexcept;
```

Returns the ordinal plural category of an integer in a locale's language, by the rules of CLDR 46 (`ordinals.xml`):
in English 1, 21 and 101 are `one` ("1st"), 2 and 22 `two` ("2nd"), 3 and 23 `few` ("3rd"), 11–13 and the rest
`other` ("11th"); a language whose ordinals have one form answers `other` for every number.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the integer, of any integral type but `bool`; a negative one by its absolute value |
| `l` | the locale whose language's rules apply |

## Return value

The category.

## Complexity

Logarithmic in the number of languages.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto en = txt::locale("en");
    const char* suffix[] = {"", "st", "nd", "rd", "", "th"};
    for (int n : {1, 2, 3, 4, 11, 21, 22, 113}) {
        print("{}{} ", n, suffix[int(txt::ordinal_of(n, en))]);
    }
    println("");
}
```

Output:

```text
1st 2nd 3rd 4th 11th 21st 22nd 113th 
```

## See also

- [plural](plural.md)
- [plural_of](plural_of.md)
- [sgcl::txt](README.md)

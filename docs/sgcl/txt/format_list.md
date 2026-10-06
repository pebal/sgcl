[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::format_list

```cpp
string format_list(const slice<const string>& items, const locale& l = {},          // (1)
                   list_type t = list_type::conjunction, width w = width::wide);
string format_list(std::initializer_list<string> items, const locale& l = {},       // (2)
                   list_type t = list_type::conjunction, width w = width::wide);
```

Returns the items joined as a locale joins a list (CLDR's list patterns, LDML Part 2): "Ala, Ola i Ela" in Polish,
"Ala, Ola, and Ela" in English. Two items take the pattern of two, three the pattern of three where the language has
one, more the patterns of the start, the middle and the end. Spanish writes "e" for "y" before the sound i ("Juan e
Irene") and "u" for "o" before the sound o ("uno u otro", "7 u 8"), Hebrew a hyphen after "ו" before a word not in
Hebrew, as ICU does.

1. Items in a slice: a `vector<string>`, an array.
2. Items written in the call: `format_list({"a", "b"}, l)`.

## Parameters

| Parameter | Description |
|---|---|
| `items` | the items, written as they are |
| `l` | the locale |
| `t` | what the list joins, a [list_type](list_type.md) |
| `w` | the width, a [width](width.md) |

## Return value

The list as text; an empty text for no items, the item itself for one.

## Complexity

Linear in the total length of the items.

## Exceptions

`length_error` when the text would be longer than a [string](../core/string/README.md) holds.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}", txt::format_list({"Ala", "Ola", "Ela"}, txt::locale("pl")));
    println("{}", txt::format_list({"red", "green", "blue"}, txt::locale("en"),
                                   txt::list_type::disjunction));
    println("{}", txt::format_list({"Juan", "Irene"}, txt::locale("es")));
}
```

Output:

```text
Ala, Ola i Ela
red, green, or blue
Juan e Irene
```

## See also

- [list_type](list_type.md)
- [width](width.md)
- [locale](locale/README.md)
- [sgcl::txt](README.md)

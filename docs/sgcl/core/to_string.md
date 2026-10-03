[sgcl](../README.md) › [core](README.md)

# sgcl::to_string

```cpp
#include "sgcl/core/string.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T>
    requires std::is_arithmetic_v<T> && (!std::is_same_v<T, bool>)
             && (!std::is_same_v<T, char>)
    string to_string(T v) noexcept;                                   // (1)
    string to_string(bool v) noexcept;                                // (2)
    string to_string(char c) noexcept;                                // (3)
}
```

Makes a [string](string.md) of a value, made once at its size.

1. A number: the text `std::to_string` writes, an integer in decimal and a floating-point number with six digits
   after the point. A character type other than `char` (`char32_t`, `wchar_t`, `signed char`, `unsigned char`) is a
   number here: `to_string(U'ż')` is `"380"`.
2. `"true"` or `"false"`.
3. A string of the one character.

## Parameters

| Parameter | Description |
|---|---|
| `v` | the number or the truth value |
| `c` | the character |

## Return value

The string.

## Complexity

Linear in the number of characters written.

## Exceptions

None.

## Notes

[parse](parse.md) reads a number back from its text. A floating-point number in its shortest form, or a number in
another form, is written by [txt::format](../txt/format.md): `txt::format("{}", 2.5)` is `"2.5"`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string line = string::concat("item ", to_string(42), ": ", to_string(2.5), " ",
                                 to_string(true), " ", to_string('x'));
    println("{}", line);
    println("{} {}", to_string(-7), to_string(U'ż'));
}
```

Output:

```text
item 42: 2.500000 true x
-7 380
```

## See also

- [parse](parse.md): a number from its text
- [string::concat](string/concat.md): one string of a few pieces
- [txt::format](../txt/format.md): text of values in a form of one's own
- [sgcl::string](string.md)

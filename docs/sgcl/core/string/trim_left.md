[sgcl](../../README.md) › [core](../README.md) › [string](../string.md)

# sgcl::string::trim_left

```cpp
/*(1)*/ basic_string trim_left() const noexcept;
/*(2)*/ basic_string trim_left(view_type chars) const noexcept;
/*(3)*/ template<size_t N>
        basic_string trim_left(const CharT (&chars)[N]) const noexcept;
/*(4)*/ basic_string trim_left(std::u32string_view set) const noexcept
            requires (!std::same_as<CharT, char32_t>);
```

Returns the string without the white space, or the characters given, at the start.

1. Trims Unicode white space, [unicode::is_space](../unicode.md): the space, the tab, the newline and the rest of the
   C locale's six, and the no-break, the ideographic and the other spaces. In a `wstring`, a `u16string` and a
   `u32string` each unit is tested as a code point.
2. Trims the characters of `chars`, each a `CharT`: in UTF-8, bytes.
3. Trims the characters of an array, a literal, read up to its first NUL or its end, whichever comes first.
4. Trims the code points of `set`: `trim_left(U"«»")`, the string walked by code points (in a `u16string` a
   surrogate pair is one). For every string but a `u32string`, whose (2) takes code points.

- (1–4) The same object when there is nothing to trim, so a result may be compared by `object()` as by `==`; the
  empty string when every character is trimmed.

## Parameters

| Parameter | Description |
|---|---|
| `chars` | the characters to trim, as a set |
| `set` | the code points to trim, as a set |

## Return value

The string without them at its start: a new string, this string's object when there were none, or the empty string
when nothing is left.

## Complexity

Linear in the number of characters trimmed, each looked up in `chars` or `set` (2–4), plus the copy of the rest when
anything was trimmed.

## Exceptions

None.

## Notes

The characters of `chars` (2–3) are bytes in UTF-8, so a letter of more than one byte given there is a set of its
bytes, which may cut the first byte off another letter; the letters beyond ASCII are given as code points (4), as
on [trim](trim.md).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string line = "\t  indented  ";
    println("[{}]", line.trim_left());

    string number = "000120";
    println("[{}] {}", number.trim_left("0"), number.trim_left(" ").object() == number.object());

    string quoted = "»«quote«";
    println("[{}]", quoted.trim_left(U"«»"));
    println("{}", string("   ").trim_left().empty());
}
```

Output:

```text
[indented  ]
[120] true
[quote«]
true
```

## See also

- [trim](trim.md): at both ends
- [trim_right](trim_right.md): at the end
- [trim_prefix](trim_prefix.md): without a prefix, once
- [sgcl::string](../string.md)

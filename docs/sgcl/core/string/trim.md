[sgcl](../../README.md) › [core](../README.md) › [string](README.md)

# sgcl::string::trim

```cpp
basic_string trim() const noexcept;                           // (1)
basic_string trim(view_type chars) const noexcept;            // (2)
template<size_t N>
basic_string trim(const CharT (&chars)[N]) const noexcept;    // (3)
basic_string trim(std::u32string_view set) const noexcept     // (4)
    requires (!std::same_as<CharT, char32_t>);
```

Returns the string without the white space, or the characters given, at both ends.

1. Trims Unicode white space, [unicode::is_space](../unicode/README.md): the space, the tab, the newline and the rest of the
   C locale's six, and the no-break, the ideographic and the other spaces. In a `wstring`, a `u16string` and a
   `u32string` each unit is tested as a code point.
2. Trims the characters of `chars`, each a `CharT`: in UTF-8, bytes.
3. Trims the characters of an array, a literal, read up to its first NUL or its end, whichever comes first.
4. Trims the code points of `set`: `trim(U"«»")`, the string walked by code points (in a `u16string` a surrogate
   pair is one). For every string but a `u32string`, whose (2) takes code points.

- (1–4) The same object when there is nothing to trim, so a result may be compared by `object()` as by `==`; the
  empty string when every character is trimmed.

## Parameters

| Parameter | Description |
|---|---|
| `chars` | the characters to trim, as a set |
| `set` | the code points to trim, as a set |

## Return value

The string without them at its ends: a new string, this string's object when there were none, or the empty string
when nothing is left.

## Complexity

Linear in the number of characters trimmed, each looked up in `chars` or `set` (2–4), plus the copy of the rest when
anything was trimmed.

## Exceptions

None.

## Notes

The characters of `chars` (2–3) are bytes in UTF-8, so a letter of more than one byte given there is a set of its
bytes: `trim("«»")` trims the bytes C2, AB and BB, and cuts the C2 off a `¢` that begins the string. The letters
beyond ASCII are given as code points (4).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string padded = "\u00A0 Łódź \t\n";  // a no-break space first
    println("[{}]", padded.trim());

    string clean = "Łódź";
    println("{}", clean.trim().object() == clean.object());

    string quoted = "«Łódź»";
    println("[{}] [{}]", quoted.trim(U"«»"), string("--x-y--").trim("-"));
    println("{}", string("¢5").trim("«»").is_valid_utf8());
    println("{}", string(" \t ").trim().empty());
}
```

Output:

```text
[Łódź]
true
[Łódź] [x-y]
false
true
```

## See also

- [trim_left](trim_left.md), [trim_right](trim_right.md): at one end only
- [trim_prefix](trim_prefix.md), [trim_suffix](trim_suffix.md): without a prefix or a suffix, once
- [fields](fields.md): the words between runs of white space
- [unicode](../unicode/README.md): what white space is
- [sgcl::string](README.md)

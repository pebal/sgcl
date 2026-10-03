[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [text](../text.md)

# sgcl::mixin::text\<Derived, CharT, Traits\>::find_first_not_of

```cpp
/*(1)*/ size_type find_first_not_of(view_type s, size_type pos = 0) const noexcept;
/*(2)*/ size_type find_first_not_of(CharT c, size_type pos = 0) const noexcept;
/*(3)*/ template<size_t N>
        size_type find_first_not_of(const CharT (&s)[N], size_type pos = 0) const noexcept;
/*(4)*/ template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        size_type find_first_not_of(P s, size_type pos = 0) const noexcept;
/*(5)*/ size_type find_first_not_of(std::u32string_view set, size_type pos = 0) const noexcept
            requires (!std::same_as<CharT, char32_t>);
/*(6)*/ size_type find_first_not_of(char32_t c, size_type pos = 0) const noexcept
            requires (!std::same_as<CharT, char32_t>);
/*(7)*/ size_type find_first_not_of(int, size_type = 0) const = delete;
```

Finds the first character at or after `pos` that is none of a set of characters.

1. The characters of `s`, as `std::basic_string_view::find_first_not_of`: each unit of `CharT` is one character, so
   in a UTF-8 text the bytes of a code point are compared one by one.
2. The character `c`: the first character other than `c`.
3. The characters of an array, a literal, up to its first NUL or its end, never past it.
4. The characters at the pointer `s`, up to their NUL.
5. A set of code points: the text is walked by code points (UTF-8 sequences, UTF-16 units and surrogate pairs)
   from the unit `pos`, and the result is the unit where the first code point not in `set` begins. A `pos` inside
   a code point (a continuation byte, the low half of a surrogate pair) starts the walk at the next one, so no
   position inside a code point is found. An invalid byte, or a surrogate without its pair, is one code point,
   U+FFFD. Takes part unless `CharT` is `char32_t`, whose view
   is (1).
6. The code point `c`: (5) with a set of one.
7. Deleted: `'ż'` in a UTF-8 source is a multi-character literal of type `int`, which would be cut to one byte. The
   call does not compile; a set of code points is written `U"ż"`.

## Parameters

| Parameter | Description |
|---|---|
| `s`, `set` | the characters to pass over |
| `c` | the character to pass over |
| `pos` | the position the search starts at; in (5–6) the first code point that begins at or after it |

## Return value

The position of the character found, a unit of `CharT` (a byte in a UTF-8 text), or `npos` when every character
from `pos` on is in the set.

## Complexity

- (1–4) Linear in the size of the text, times the size of the set at worst.
- (5–6) Linear in the number of code points walked, times the size of the set.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string s = "0007, 42";
    println("{} {}", s.find_first_not_of('0'), s.find_first_not_of("0123456789"));
    println("{}", s.find_first_not_of(", ", 4));

    string title = "\u3000 Łódź";
    println("{} {}", title.find_first_not_of(U" \u3000"), title.find_first_not_of(U" \u3000Ł"));
}
```

Output:

```text
3 4
6
4 6
```

## See also

- [find_last_not_of](find_last_not_of.md): the last character that is not in a set
- [find_first_of](find_first_of.md): the first character that is in a set
- [sgcl::mixin::text\<Derived, CharT, Traits\>](../text.md)

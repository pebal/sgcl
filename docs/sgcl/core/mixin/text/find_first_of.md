[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [text](../text.md)

# sgcl::mixin::text\<Derived, CharT, Traits\>::find_first_of

```cpp
/*(1)*/ size_type find_first_of(view_type s, size_type pos = 0) const noexcept;
/*(2)*/ size_type find_first_of(CharT c, size_type pos = 0) const noexcept;
/*(3)*/ template<size_t N>
        size_type find_first_of(const CharT (&s)[N], size_type pos = 0) const noexcept;
/*(4)*/ template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        size_type find_first_of(P s, size_type pos = 0) const noexcept;
/*(5)*/ size_type find_first_of(std::u32string_view set, size_type pos = 0) const noexcept
            requires (!std::same_as<CharT, char32_t>);
/*(6)*/ size_type find_first_of(char32_t c, size_type pos = 0) const noexcept
            requires (!std::same_as<CharT, char32_t>);
/*(7)*/ size_type find_first_of(int, size_type = 0) const = delete;
```

Finds the first character at or after `pos` that is one of a set of characters.

1. The characters of `s`, as `std::basic_string_view::find_first_of`: each unit of `CharT` is one character, so in
   a UTF-8 text a byte of a code point of two or more bytes matches by itself.
2. The character `c`: the same as `find(c, pos)`.
3. The characters of an array, a literal, up to its first NUL or its end, never past it.
4. The characters at the pointer `s`, up to their NUL.
5. A set of code points: the text is walked by code points (UTF-8 sequences, UTF-16 units and surrogate pairs)
   from the unit `pos`, and the result is the unit where the first code point in `set` begins. A `pos` inside a
   code point (a continuation byte, the low half of a surrogate pair) starts the walk at the next one, so no
   position inside a code point is found.
   `find_first_of(U"«»")` finds a guillemet, not a byte of one. An invalid byte, or a surrogate without its pair,
   is one code point, U+FFFD. Takes part unless `CharT` is `char32_t`, whose view is (1).
6. The code point `c`: (5) with a set of one.
7. Deleted: `'ż'` in a UTF-8 source is a multi-character literal of type `int`, which would be cut to one byte. The
   call does not compile; a set of code points is written `U"ż"`.

## Parameters

| Parameter | Description |
|---|---|
| `s`, `set` | the characters to look for |
| `c` | the character to look for |
| `pos` | the position the search starts at; in (5–6) the first code point that begins at or after it |

## Return value

The position of the character found, a unit of `CharT` (a byte in a UTF-8 text), or `npos` when there is none.

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
    string s = "the quick brown fox";
    println("{} {}", s.find_first_of("xyz"), s.find_first_of("aeiou", 6));

    string quote = "Powiedział: «żółw»";
    println("{} {}", quote.find_first_of(U"«»"), quote.find_first_of(U"«»", 15));
    println("{}", quote.find_first_of(U"żółw"));
}
```

Output:

```text
18 6
13 22
2
```

## See also

- [find_last_of](find_last_of.md): the last character that is in a set
- [find_first_not_of](find_first_not_of.md): the first character that is not in a set
- [find](find.md): the first occurrence of a substring or a character
- [sgcl::mixin::text\<Derived, CharT, Traits\>](../text.md)

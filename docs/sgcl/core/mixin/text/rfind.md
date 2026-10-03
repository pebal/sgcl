[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [text](../text.md)

# sgcl::mixin::text\<Derived, CharT, Traits\>::rfind

```cpp
/*(1)*/ size_type rfind(view_type s, size_type pos = npos) const noexcept;
/*(2)*/ size_type rfind(CharT c, size_type pos = npos) const noexcept;
/*(3)*/ size_type rfind(const CharT* s, size_type pos, size_type n) const noexcept;
/*(4)*/ template<size_t N>
        size_type rfind(const CharT (&s)[N], size_type pos = npos) const noexcept;
/*(5)*/ template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        size_type rfind(P s, size_type pos = npos) const noexcept;
/*(6)*/ size_type rfind(char32_t c, size_type pos = npos) const noexcept
            requires (!std::same_as<CharT, char32_t>);
/*(7)*/ size_type rfind(int, size_type = npos) const = delete;
```

Finds the last occurrence of a substring or a character that begins at or before `pos`, as
`std::basic_string_view::rfind`.

1. The substring `s`: a string, a text slice, a `std::basic_string_view`.
2. The character `c`.
3. The first `n` characters at `s`, which may hold a NUL.
4. The characters of an array, a literal, up to its first NUL or its end, never past it.
5. The characters at the pointer `s`, up to their NUL.
6. The code point `c`, encoded into the text's units and searched as a substring: its bytes in a UTF-8 text, where
   `rfind(U'ł')` is `rfind("ł")`, one unit or a surrogate pair in a UTF-16 one; the position is the unit where the
   code point begins. A value that is no code point (a surrogate, past U+10FFFF) has no encoding and is found
   nowhere, not as the U+FFFD it would be written as. Takes part unless `CharT` is `char32_t`, whose code point is
   the character of (2).
7. Deleted: `'ł'` in a UTF-8 source is a multi-character literal of type `int`, which would be cut to one byte. The
   call does not compile; the character is written `U'ł'`, or as text, `"ł"`.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the substring to look for |
| `c` | the character to look for |
| `pos` | the last position an occurrence may begin at; `npos` searches the whole text |
| `n` | the length of the substring at `s` |

## Return value

The position of the first character of the occurrence, a unit of `CharT` (a byte in a UTF-8 text), or `npos` when
there is none.

## Complexity

Linear in the size of the text, times the length of the substring at worst.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string s = "Wisła, Łódź, Wisła";
    println("{} {} {}", s.rfind("Wisła"), s.rfind(U'ł'), s.rfind(U'ł', 10));
    println("{} {}", s.rfind(','), s.rfind(U'x') == string::npos);
}
```

Output:

```text
17 20 3
15 true
```

## See also

- [find](find.md): the first occurrence
- [find_last_of](find_last_of.md): the last character that is in a set
- [sgcl::mixin::text\<Derived, CharT, Traits\>](../text.md)

[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [text](README.md)

# sgcl::mixin::text\<Derived, CharT, Traits\>::find

```cpp
size_type find(view_type s, size_type pos = 0) const noexcept;                               // (1)
size_type find(CharT c, size_type pos = 0) const noexcept;                                   // (2)
size_type find(const CharT* s, size_type pos, size_type n) const noexcept;                   // (3)
template<size_t N> size_type find(const CharT (&s)[N], size_type pos = 0) const noexcept;    // (4)
template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
size_type find(P s, size_type pos = 0) const noexcept;                                       // (5)
size_type find(char32_t c, size_type pos = 0) const noexcept                                 // (6)
    requires (!std::same_as<CharT, char32_t>);
size_type find(int, size_type = 0) const = delete;                                           // (7)
```

Finds the first occurrence of a substring or a character at or after `pos`, as `std::basic_string_view::find`.

1. The substring `s`: a string, a text slice, a `std::basic_string_view`.
2. The character `c`.
3. The first `n` characters at `s`, which may hold a NUL.
4. The characters of an array, a literal, up to its first NUL or its end, never past it.
5. The characters at the pointer `s`, up to their NUL.
6. The code point `c`, encoded into the text's units and searched as a substring: its bytes in a UTF-8 text, where
   `find(U'ż')` is `find("ż")`, one unit or a surrogate pair in a UTF-16 one; the position is the unit where the
   code point begins. A value that is no code point (a surrogate, past U+10FFFF) has no encoding and is found
   nowhere, not as the U+FFFD it would be written as. Takes part unless `CharT` is `char32_t`, whose code point is
   the character of (2).
7. Deleted: `'ż'` in a UTF-8 source is a multi-character literal of type `int`, which would be cut to one byte. The
   call does not compile; the character is written `U'ż'`, or as text, `"ż"`.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the substring to look for |
| `c` | the character to look for |
| `pos` | the position the search starts at |
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
    string s = " ŁÓDŹ nad Wisłą";
    println("{} {} {}", s.find(U'Ó'), s.find(U'Ź'), s.find("nad"));
    println("{} {}", s.find('a', 10), s.find(U'x') == string::npos);

    slice<const char> rest = s.as_slice(8);
    println("{} {}", rest.find("Wis"), rest.find(U'ł'));

    u16string u = u"a😀b";  // the emoji a surrogate pair, at 1 and 2
    println("{} {}", u.find(U'😀'), u.find(U'b'));
}
```

Output:

```text
3 6 9
10 true
5 8
1 3
```

## See also

- [rfind](rfind.md): the last occurrence
- [find_first_of](find_first_of.md): the first character that is in a set
- [contains](contains.md): whether there is an occurrence
- [sgcl::mixin::text\<Derived, CharT, Traits\>](README.md)

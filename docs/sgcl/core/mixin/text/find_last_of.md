[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [text](README.md)

# sgcl::mixin::text\<Derived, CharT, Traits\>::find_last_of

```cpp
size_type find_last_of(view_type s, size_type pos = npos) const noexcept;               // (1)
size_type find_last_of(CharT c, size_type pos = npos) const noexcept;                   // (2)
template<size_t N>
size_type find_last_of(const CharT (&s)[N], size_type pos = npos) const noexcept;       // (3)
template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
size_type find_last_of(P s, size_type pos = npos) const noexcept;                       // (4)
size_type find_last_of(std::u32string_view set, size_type pos = npos) const noexcept    // (5)
    requires (!std::same_as<CharT, char32_t>);
size_type find_last_of(char32_t c, size_type pos = npos) const noexcept                 // (6)
    requires (!std::same_as<CharT, char32_t>);
size_type find_last_of(int, size_type = npos) const = delete;                           // (7)
```

Finds the last character at or before `pos` that is one of a set of characters.

1. The characters of `s`, as `std::basic_string_view::find_last_of`: each unit of `CharT` is one character, so in
   a UTF-8 text a byte of a code point of two or more bytes matches by itself.
2. The character `c`: the same as `rfind(c, pos)`.
3. The characters of an array, a literal, up to its first NUL or its end, never past it.
4. The characters at the pointer `s`, up to their NUL.
5. A set of code points: the text is walked backwards by code points (UTF-8 sequences, UTF-16 units and surrogate
   pairs), from its end or, with a `pos` inside it, from the code point that begins at or before `pos`, one that
   `pos` cuts included; the result is the unit where the last code point in `set` begins. An invalid byte, or a
   surrogate without its pair, is one code point, U+FFFD. Takes part unless `CharT` is `char32_t`, whose view is
   (1).
6. The code point `c`: (5) with a set of one.
7. Deleted: `'ż'` in a UTF-8 source is a multi-character literal of type `int`, which would be cut to one byte. The
   call does not compile; a set of code points is written `U"ż"`.

## Parameters

| Parameter | Description |
|---|---|
| `s`, `set` | the characters to look for |
| `c` | the character to look for |
| `pos` | the last position looked at; `npos` searches the whole text |

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
    string path = "/usr/local/bin/tool";
    println("{} {}", path.find_last_of('/'), path.find_last_of("/.", 10));

    string quote = "«żółw» i «wąż»";
    println("{} {}", quote.find_last_of(U"«»"), quote.find_last_of(U"ółą"));

    string word = "aŁb";  // Ł at 1 and 2: position 1 cuts it, and it is looked at
    println("{}", word.find_last_of(U"Ł", 1));
}
```

Output:

```text
14 10
21 17
1
```

## See also

- [find_first_of](find_first_of.md): the first character that is in a set
- [find_last_not_of](find_last_not_of.md): the last character that is not in a set
- [rfind](rfind.md): the last occurrence of a substring or a character
- [sgcl::mixin::text\<Derived, CharT, Traits\>](README.md)

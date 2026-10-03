[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [text](../text.md)

# sgcl::mixin::text\<Derived, CharT, Traits\>::contains

```cpp
bool contains(view_type s) const noexcept;                                             // (1)
bool contains(CharT c) const noexcept;                                                 // (2)
template<size_t N> bool contains(const CharT (&s)[N]) const noexcept;                  // (3)
template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
bool contains(P s) const noexcept;                                                     // (4)
bool contains(char32_t c) const noexcept requires (!std::same_as<CharT, char32_t>);    // (5)
bool contains(int) const = delete;                                                     // (6)
```

Checks whether the text contains a substring or a character: `find(s) != npos`.

1. The substring `s`: a string, a text slice, a `std::basic_string_view`.
2. The character `c`.
3. The characters of an array, a literal, up to its first NUL or its end, never past it.
4. The characters at the pointer `s`, up to their NUL.
5. The code point `c`, encoded into the text's units and searched as a substring: its bytes in a UTF-8 text, where
   `contains(U'😀')` is `contains("😀")`, one unit or a surrogate pair in a UTF-16 one. A value that is no code
   point (a surrogate, past U+10FFFF) has no encoding and is in no text, not as the U+FFFD it would be written as.
   Takes part unless `CharT` is `char32_t`, whose code point is the character of (2).
6. Deleted: `'ż'` in a UTF-8 source is a multi-character literal of type `int`, which would be cut to one byte. The
   call does not compile; the character is written `U'ż'`, or as text, `"ż"`.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the substring to look for |
| `c` | the character to look for |

## Return value

`true` when the text contains the substring or the character, `false` otherwise. Every text contains the empty one.

## Complexity

Linear in the size of the text, times the length of the substring at worst.

## Exceptions

None.

## Notes

On a text slice, `contains` is `mixin::text`'s (a substring or a character), not
[mixin::enumerable](../enumerable.md)'s of an element: the slice says which, since a name in two bases is
ambiguous. It declares its own `contains` of a view, of a `CharT`, of a `char32_t` and a deleted one of an `int`,
which call these.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string s = "Łódź nad Wisłą 😀";
    println("{} {} {}", s.contains("nad"), s.contains(U'ą'), s.contains(U'ż'));
    println("{} {}", s.contains('W'), s.contains(U'😀'));

    slice<const char> word = s.as_slice(0, 7);
    println("{} {}", word.contains("ódź"), word.contains(U'W'));
}
```

Output:

```text
true true false
true true
true false
```

## See also

- [find](find.md): the position of a substring or a character
- [starts_with](starts_with.md), [ends_with](ends_with.md): the substring at the start, at the end
- [mixin::enumerable](../enumerable.md): `contains` of an element, in the other containers
- [sgcl::mixin::text\<Derived, CharT, Traits\>](../text.md)

[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [text](../text.md)

# sgcl::mixin::text\<Derived, CharT, Traits\>::ends_with

```cpp
bool ends_with(view_type s) const noexcept;                                             // (1)
bool ends_with(CharT c) const noexcept;                                                 // (2)
template<size_t N> bool ends_with(const CharT (&s)[N]) const noexcept;                  // (3)
template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
bool ends_with(P s) const noexcept;                                                     // (4)
bool ends_with(char32_t c) const noexcept requires (!std::same_as<CharT, char32_t>);    // (5)
bool ends_with(int) const = delete;                                                     // (6)
```

Checks whether the text ends with the given suffix.

1. The suffix `s`: a string, a text slice, a `std::basic_string_view`.
2. The character `c`.
3. The characters of an array, a literal, up to its first NUL or its end, never past it.
4. The characters at the pointer `s`, up to their NUL.
5. The code point `c`, encoded into the text's units: its bytes in a UTF-8 text, where `ends_with(U'ł')` is
   `ends_with("ł")`, one unit or a surrogate pair in a UTF-16 one. A value that is no code point (a surrogate, past
   U+10FFFF) has no encoding and ends no text. Takes part unless `CharT` is `char32_t`, whose code point is the
   character of (2).
6. Deleted: `'ł'` in a UTF-8 source is a multi-character literal of type `int`, which would be cut to one byte. The
   call does not compile; the character is written `U'ł'`, or as text, `"ł"`.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the suffix |
| `c` | the character |

## Return value

`true` when the text ends with the suffix, `false` otherwise. Every text ends with the empty one.

## Complexity

Linear in the length of the suffix.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string s = "żółw";
    println("{} {} {}", s.ends_with("łw"), s.ends_with(U'w'), s.ends_with(U'ł'));

    string file = "notes.txt";
    const char* extension = ".txt";
    println("{} {}", file.ends_with('t'), file.ends_with(extension));
    println("{}", file.as_slice(0, 5).ends_with("es"));
}
```

Output:

```text
true true false
true true
true
```

## See also

- [starts_with](starts_with.md): checks whether the text starts with a prefix
- [rfind](rfind.md): the position of the last occurrence of a substring or a character
- [sgcl::mixin::text\<Derived, CharT, Traits\>](../text.md)

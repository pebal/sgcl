[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [text](../text.md)

# sgcl::mixin::text\<Derived, CharT, Traits\>::starts_with

```cpp
bool starts_with(view_type s) const noexcept;                                             // (1)
bool starts_with(CharT c) const noexcept;                                                 // (2)
template<size_t N> bool starts_with(const CharT (&s)[N]) const noexcept;                  // (3)
template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
bool starts_with(P s) const noexcept;                                                     // (4)
bool starts_with(char32_t c) const noexcept requires (!std::same_as<CharT, char32_t>);    // (5)
bool starts_with(int) const = delete;                                                     // (6)
```

Checks whether the text begins with the given prefix.

1. The prefix `s`: a string, a text slice, a `std::basic_string_view`.
2. The character `c`.
3. The characters of an array, a literal, up to its first NUL or its end, never past it.
4. The characters at the pointer `s`, up to their NUL.
5. The code point `c`, encoded into the text's units: its bytes in a UTF-8 text, where `starts_with(U'ż')` is
   `starts_with("ż")`, one unit or a surrogate pair in a UTF-16 one. A value that is no code point (a surrogate,
   past U+10FFFF) has no encoding and begins no text. Takes part unless `CharT` is `char32_t`, whose code point is
   the character of (2).
6. Deleted: `'ż'` in a UTF-8 source is a multi-character literal of type `int`, which would be cut to one byte. The
   call does not compile; the character is written `U'ż'`, or as text, `"ż"`.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the prefix |
| `c` | the character |

## Return value

`true` when the text begins with the prefix, `false` otherwise. Every text begins with the empty one.

## Complexity

Linear in the length of the prefix.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string s = "żółw";
    println("{} {} {}", s.starts_with("żó"), s.starts_with(U'ż'), s.starts_with(U'ó'));

    string path = "/usr/local/bin";
    const char* root = "/usr";
    println("{} {}", path.starts_with('/'), path.starts_with(root));
    println("{}", path.as_slice(5).starts_with("local"));
}
```

Output:

```text
true true false
true true
true
```

## See also

- [ends_with](ends_with.md): checks whether the text ends with a suffix
- [find](find.md): the position of a substring or a character
- [sgcl::mixin::text\<Derived, CharT, Traits\>](../text.md)

[sgcl](../../README.md) › [core](../README.md) › [string](../string.md)

# sgcl::string::replace

```cpp
/*(1)*/ basic_string replace(view_type from, view_type to, size_type count = 0) const;
/*(2)*/ template<size_t N, size_t M>
        basic_string replace(const CharT (&from)[N], const CharT (&to)[M],
                             size_type count = 0) const;
/*(3)*/ template<size_t N>
        basic_string replace(const CharT (&from)[N], view_type to, size_type count = 0) const;
/*(4)*/ template<size_t M>
        basic_string replace(view_type from, const CharT (&to)[M], size_type count = 0) const;
/*(5)*/ basic_string replace(CharT from, CharT to, size_type count = 0) const;
/*(6)*/ basic_string replace(char32_t from, char32_t to, size_type count = 0) const
            requires (!std::same_as<CharT, char32_t>);
/*(7)*/ basic_string replace(int, int, size_type = 0) const = delete;
```

Returns the string with the occurrences of `from` replaced by `to`: every one, or the first `count` of them.

1. Replaces a text by a text, each given as a view: a string, a slice, a `std::string`, a pointer up to its NUL
   convert to it.
2. Replaces an array, a literal, by an array.
3. Replaces an array by a view.
4. Replaces a view by an array.
5. Replaces a character by a character.
6. Replaces a code point by a code point, the units of one by those of the other, which may differ in length: the
   bytes of their UTF-8 in a `string` and a `u8string`, `replace(U'Ó', U'o')`, one unit or a surrogate pair in a
   `u16string`. A `from` that is no code point (a surrogate, past U+10FFFF) occurs nowhere, not as the U+FFFD it
   would be written as, and the result is this string's object; a `to` that is none is written as U+FFFD, as
   `utf8::encode` writes it. For every string but a `u32string`, whose (5) takes code points.
7. Deleted. `'ż'` in a UTF-8 source is, where a compiler takes it, a multi-character literal of type `int`, two bytes in
   one value, so `replace('ż', 'z')` does not compile; `replace(U'ż', U'z')` is (6).

- (2–4) An array is read up to its first NUL or its end, whichever comes first.
- (1–6) The occurrences are found left to right without overlapping, and what was put in is never searched again:
  `"aaa"` with `"aa"` replaced by `"b"` is `"ba"`, and `"aa"` with `"a"` replaced by `"aa"` is `"aaaa"`. An empty
  `from` changes nothing, where Go's `strings.Replace` puts `to` at the start and after every code point. The same
  object when `from` is empty or does not occur, so a result may be compared by `object()` as by `==`.

## Parameters

| Parameter | Description |
|---|---|
| `from` | the text, the character or the code point to replace |
| `to` | what to put in its place |
| `count` | how many occurrences to replace, the first ones; 0, the default, is every one |

## Return value

The new string with the occurrences replaced; this string's object when `from` is empty or does not occur.

## Complexity

Linear in the length of the string, plus linear in the length of the result: one pass counts the occurrences, the
second writes the result once, into a string of the size they make.

## Exceptions

`length_error` when the result would pass `max_size()`, checked before the length is computed, so a long `to` times
many occurrences is refused rather than wrapped past 64 bits.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string title = "the quick brown fox";
    println("{}", title.replace(" ", "_"));
    println("{}", title.replace(' ', '-', 2));
    println("{}", string("ŁÓDŹ").replace(U'Ó', U'o'));
    println("{} {}", string("aaa").replace("aa", "b"), string("aa").replace("a", "aa"));
    println("{}", title.replace("slow", "fast").object() == title.object());
}
```

Output:

```text
the_quick_brown_fox
the-quick-brown fox
ŁoDŹ
ba aaaa
true
```

## See also

- [trim_prefix](trim_prefix.md), [trim_suffix](trim_suffix.md): without a prefix or a suffix
- [split](split.md), [join](join.md): the pieces between the occurrences, and one string of them
- [mixin::text](../mixin/text.md): `find`, `contains`
- [sgcl::string](../string.md)

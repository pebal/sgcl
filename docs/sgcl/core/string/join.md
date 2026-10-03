[sgcl](../../README.md) › [core](../README.md) › [string](README.md)

# sgcl::string::join

```cpp
template<std::ranges::input_range R>
requires std::is_convertible_v<std::ranges::range_reference_t<R>, view_type>
static basic_string join(R&& parts, view_type sep);                             // (1)
template<std::ranges::input_range R>
requires std::is_convertible_v<std::ranges::range_reference_t<R>, view_type>
static basic_string join(R&& parts, CharT sep);                                 // (2)
template<std::ranges::input_range R, size_t N>
requires std::is_convertible_v<std::ranges::range_reference_t<R>, view_type>
static basic_string join(R&& parts, const CharT (&sep)[N]);                     // (3)
template<std::ranges::input_range R, class P>
requires std::is_convertible_v<std::ranges::range_reference_t<R>, view_type>
      && (std::same_as<P, const CharT*> || std::same_as<P, CharT*>)
static basic_string join(R&& parts, P sep);                                     // (4)
template<std::ranges::input_range R>
requires std::is_convertible_v<std::ranges::range_reference_t<R>, view_type>
      && (!std::same_as<CharT, char32_t>)
static basic_string join(R&& parts, char32_t sep);                              // (5)
template<std::ranges::input_range R>
static basic_string join(R&&, int) = delete;                                    // (6)
```

Makes one string of the parts, in order, with `sep` between each two. A static function: `string::join(names, ", ")`.
The parts are the elements of any input range whose elements a `view_type` is made of: strings, slices, literals,
`std::string`s, `std::string_view`s, and the range of [split](split.md) and [fields](fields.md), taken as it is.

1. Puts the characters of a view between the parts.
2. Puts a character between the parts.
3. Puts the characters of an array, a literal, read up to its first NUL or its end, whichever comes first.
4. Puts the characters a pointer points to, up to their NUL. Takes part only for `CharT*` and `const CharT*`:
   `nullptr` and every other pointer do not compile.
5. Puts a code point, its units: the bytes of its UTF-8 in a `string` and a `u8string`, `join(parts, U'·')`, one
   unit or a surrogate pair in a `u16string`; a value that is no code point (a surrogate, past U+10FFFF) as U+FFFD,
   as `utf8::encode` writes it. For every string but a `u32string`, whose (2) takes a code point.
6. Deleted. `'ż'` in a UTF-8 source is, where a compiler takes it, a multi-character literal of type `int`, two bytes in
   one value, so `join(parts, 'ż')` does not compile; `join(parts, U'ż')` is (5).

A forward range is walked twice: the lengths of the parts are summed, then each part and separator is written once
into a string of that size, one allocation. A single-pass range (a `std::ranges::istream_view`) is gathered in a
`std::basic_string` first, its length not being known, and the string is made of that.

## Parameters

| Parameter | Description |
|---|---|
| `parts` | the range of the parts |
| `sep` | the separator put between each two parts: a text, a character or a code point |

## Return value

The new string of the parts and the separators; the empty string when there are no parts.

## Complexity

Linear in the length of the result: a forward range walked twice, a single-pass range once and its gathered
characters copied once more.

## Exceptions

`length_error` when the result would pass `max_size()`: the lengths are summed with that check at every part, so a
range that gives one long view many times over, or very many parts with a long separator, is refused before the
sum could pass 64 bits.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <list>
#include <sstream>

using namespace sgcl;

int main() {
    vector<string> names = {"alice", "bob", "carol"};
    println("{}", string::join(names, ", "));

    std::list<std::string> words = {"one", "two"};
    println("{}", string::join(words, U'·'));

    string csv = "a,b,,c";
    println("{}", string::join(csv.split(','), '|'));

    std::istringstream in("x y z");
    println("{}", string::join(std::ranges::istream_view<std::string>(in), "->"));
    println("{}", string::join(vector<string>(), ", ").empty());
}
```

Output:

```text
alice, bob, carol
one·two
a|b||c
x->y->z
true
```

## See also

- [concat](concat.md): one string of a few known pieces
- [split](split.md): the pieces between the occurrences of a separator
- [sgcl::string](README.md)

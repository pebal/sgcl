[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md)

# sgcl::mixin::text\<Derived, CharT, Traits\>

```cpp
#include "sgcl/core/mixin/text.h"   // or "sgcl/core.h"

namespace sgcl::mixin {
    template<class Derived, class CharT, class Traits = std::char_traits<CharT>>
    class text;
}
```

`mixin::text<Derived, CharT>` gives whatever holds characters through `data()` and `size()` the read side of
`std::string_view` as members: `view()` and the conversion to a `std::basic_string_view`, `length`, `at`, `copy`,
`compare`, `starts_with`, `ends_with`, `contains`, `find`, `rfind`, `find_first_of` and the rest, `str()` as a
`std::basic_string`, `npos`. [string](../../string/README.md) and [slice\<const CharT\>](../../slice/README.md) carry it, so a piece of
a string answers what the string does; the operations that make a new object (`substr`, `trim`, `split`) stay with
the class, whose type they return. Every operation runs on a `std::basic_string_view` over the characters. Like
every mixin, it is a static interface, with no virtual method and no state; its constructor and destructor are
protected, so it exists only as the base of the class that names itself in it.

The text is Unicode ([utf8](../../utf8/README.md)): UTF-8 in a `char` or `char8_t` text, whose `size()` counts bytes; UTF-16
in a `char16_t` text (and a `wchar_t` one where `wchar_t` has 16 bits), whose `size()` counts units and where a code
point past U+FFFF is a surrogate pair; a code point a unit in a `char32_t` text (and a 32-bit `wchar_t` one). The
mixin adds `runes()`, `rune_count()`, `decode(pos)`, `is_valid_utf8()`; in every text but a `char32_t` one a
`char32_t` as a character wherever a `CharT` is (`find(U'ż')`, `contains`, `rfind`, `starts_with`, `ends_with`,
`find_first_of` and the other three), encoded into the text's units (a value that is no code point is found
nowhere), and a `std::u32string_view` as a set of characters where a view is (`find_first_of(U"«»")` and the other
three), the text walked by code points;
`equal_fold`; and it deletes the `int` overloads, because `'ż'` in a UTF-8 source is an `int`. The
white space `trim()` and `fields()` skip and the case `to_lower()` maps are Unicode's (`unicode::is_space`,
`unicode::to_lower`), taken a code point at a time, a UTF-16 surrogate pair one letter; the white space of a wide
string, all of it below U+FFFF, a unit at a time.

A text given to a member as an array of characters (a literal) is read up to its first NUL or its end, never past
it; as a pointer (`CharT*` or `const CharT*`, no other) up to its NUL. An array does not decay into the pointer:
an array filled to the brim has no NUL and is not read past its end.

## Template parameters

| Parameter | Description |
|---|---|
| `Derived` | The class that carries the mixin and names itself: `basic_string<CharT, Traits>`, `slice<const CharT>` (and `slice<CharT>`). It gives `data()` and `size()`, the characters every member reads; `runes()` takes its `as_slice()` as well. |
| `CharT` | The character type: `char`, `char8_t` (UTF-8: a code point in one to four units), `wchar_t`, `char16_t`, `char32_t`. The members declared with `requires (sizeof(CharT) == 1)` are for the one-byte types alone, those with `requires (!std::same_as<CharT, char32_t>)` for every type but `char32_t`, whose `CharT` overloads take a code point already. |
| `Traits` | The character traits of the view, `std::char_traits<CharT>` by default. |

## Member types

| Type | Definition |
|---|---|
| `view_type` | `std::basic_string_view<CharT, Traits>` |
| `size_type` | `size_t` |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `npos` | `view_type::npos` | what a search returns when it finds nothing, and the position or length that means "to the end"; `static constexpr size_type` |

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | protected and defaulted: the mixin exists only as a base |
| `(destructor)` | protected and defaulted |

#### Element access

| Function | Description |
|---|---|
| [at](at.md) | access the character at a position, with bounds checking |

#### Capacity

| Function | Description |
|---|---|
| [length](length.md) | the number of characters, `size()` |

#### Operations

| Function | Description |
|---|---|
| [view, operator view_type](view.md) | the characters as a `std::basic_string_view` |
| [str](str.md) | the characters as a `std::basic_string` |
| [copy](copy.md) | copies characters into an array |
| [compare](compare.md) | compares with another text |
| [starts_with](starts_with.md) | checks whether the text starts with a prefix |
| [ends_with](ends_with.md) | checks whether the text ends with a suffix |
| [contains](contains.md) | checks whether the text contains a substring or a character |

#### Search

| Function | Description |
|---|---|
| [find](find.md) | finds the first occurrence of a substring or a character |
| [rfind](rfind.md) | finds the last occurrence of a substring or a character |
| [find_first_of](find_first_of.md) | finds the first character that is in a set |
| [find_last_of](find_last_of.md) | finds the last character that is in a set |
| [find_first_not_of](find_first_not_of.md) | finds the first character that is not in a set |
| [find_last_not_of](find_last_not_of.md) | finds the last character that is not in a set |

#### Unicode

| Function | Description |
|---|---|
| [runes](runes.md) | the code points of a UTF-8 text, decoded as they are walked |
| [rune_count](rune_count.md) | the number of code points |
| [decode](decode.md) | the code point at a byte position, with its width |
| [is_valid_utf8](is_valid_utf8.md) | checks whether every sequence of a UTF-8 text is valid |
| [equal_fold](equal_fold.md) | checks whether two texts are the same letters in either case |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator\<=\>](operator_cmp.md) | compare the text with a view, a literal or a pointer, by the characters |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string_view>

using namespace sgcl;

int main() {
    string s = "key = value";
    slice<const char> v = s.as_slice(6);
    std::string_view view = v;  // the conversion: what a std interface takes
    println("{}", view);
    println("{} {} {} {}", s.starts_with("key"), v.contains('a'), v.find("lu"), s.compare(v) < 0);
}
```

Output:

```text
value
true true 2 true
```

## See also

- [the mixins and the requirements](../README.md)
- [string](../../string/README.md), [slice](../../slice/README.md): the classes that carry it
- [utf8, unicode, runes](../../utf8/README.md): the encoding and the properties the Unicode members stand on

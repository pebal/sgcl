# sgcl::mixin::text

```cpp
#include "sgcl/core/mixin/text.h"   // or "sgcl/core/mixin/mixin.h", "sgcl/sgcl.h"

namespace sgcl::mixin {
    template<class Derived, class CharT, class Traits = std::char_traits<CharT>>
    class text;
}
```

`mixin::text<Derived, CharT>` gives whatever holds characters through `data()` and `size()` the read side of `std::string_view` as members: `view()` and the conversion to a `std::basic_string_view`, `length`, `at`, `copy`, `compare`, `starts_with`, `ends_with`, `contains`, `find`, `rfind`, `find_first_of` and the rest, `str()` as a `std::basic_string`, `npos`. [string](../string.md) and [slice\<const CharT\>](../slice.md) carry it, so a piece of a string answers what the string does; the operations that make a new object (`substr`, `trim`, `split`) stay with the class, whose type they return. Every operation runs on a `std::basic_string_view` over the characters. The text is Unicode ([utf8](../utf8.md)): the mixin adds `runes()`, `rune_count()`, `decode(pos)`, `is_valid_utf8()`, a `char32_t` as a character wherever a `CharT` is (`find(U'ż')`, `contains`, `rfind`, `starts_with`, `ends_with`), a `std::u32string_view` as a set of characters where a view is (`find_first_of(U"«»")` and the other three), `equal_fold`; and it deletes the `int` overloads, because `'ż'` in a UTF-8 source is an `int`. The white space `trim()` and `fields()` skip and the case `to_lower()` maps are Unicode's (`unicode::is_space`, `unicode::to_lower`), a code point at a time in UTF-8, a unit at a time in a wide string.

```cpp
string s = "key = value";
slice<const char> v = s.as_slice(6);        // "value"
assert(s.starts_with("key") && v.contains('a') && v.find("lu") == 2 && s.compare(v) < 0);
std::string_view std_view = v;                    // the conversion: what a std interface takes
```

On a text slice, `contains` is `mixin::text`'s (a substring or a character), not [mixin::enumerable](enumerable.md)'s of an element: the slice says which, since a name in two bases is ambiguous.

## See also

- [the mixins and the requirements](README.md), [string](../string.md), [slice](../slice.md), [utf8, unicode, runes](../utf8.md)

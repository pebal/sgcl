# sgcl::m_text

```cpp
#include "sgcl/core/mixin/m_text.h"   // or "sgcl/core/mixin/mixin.h", "sgcl/sgcl.h"

namespace sgcl {
    template<class Derived, class CharT, class Traits = std::char_traits<CharT>>
    class m_text;
}
```

`m_text<Derived, CharT>` gives whatever holds characters through `data()` and `size()` the read side of `std::string_view` as members: `view()` and the conversion to a `std::basic_string_view`, `length`, `at`, `copy`, `compare`, `starts_with`, `ends_with`, `contains`, `find`, `rfind`, `find_first_of` and the rest, `str()` as a `std::basic_string`, `npos`. [string](../string.md) and [slice\<const CharT\>](../slice.md) carry it, so a piece of a string answers what the string does; the operations that make a new object (`substr`, `trim`, `split`) stay with the class, whose type they return. Every operation runs on a `std::basic_string_view` over the characters.

```cpp
sgcl::string s = "key = value";
sgcl::slice<const char> v = s.as_slice(6);        // "value"
assert(s.starts_with("key") && v.contains('a') && v.find("lu") == 2 && s.compare(v) < 0);
std::string_view std_view = v;                    // the conversion: what a std interface takes
```

On a text slice, `contains` is `m_text`'s (a substring or a character), not [m_enumerable](m_enumerable.md)'s of an element: the slice says which, since a name in two bases is ambiguous.

## See also

- [the mixins and the concepts](README.md), [string](../string.md), [slice](../slice.md)

[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [text](../text.md)

# sgcl::mixin::text\<Derived, CharT, Traits\>::view, operator view_type

```cpp
view_type view() const noexcept;        // (1)
operator view_type() const noexcept;    // (2)
```

Returns the characters as a `std::basic_string_view<CharT, Traits>`: `data()` and `size()` of the class that carries
the mixin. What the algorithms of the mixin run on, and what a `std` interface takes.

1. The view, by name.
2. The same view, by an implicit conversion: a string or a text slice is passed where a `std::basic_string_view` is
   expected, with nothing written.

## Parameters

None.

## Return value

A view of the characters.

## Complexity

Constant.

## Exceptions

None.

## Notes

The view holds nothing: it is valid while some string or slice holds the characters, and one taken from a
temporary dangles as it would from a `std::string`. A [slice](../../slice.md) of the text holds its object, and is
the piece to keep.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string_view>

using namespace sgcl;

// a function of a std interface: it takes a std::string_view
static size_t occurrences(std::string_view text, char c) {
    size_t n = 0;
    for (char x : text) {
        n += x == c;
    }
    return n;
}

int main() {
    string s = "hello, world";
    std::string_view whole = s.view();
    println("{} {}", whole.size(), whole.substr(7));
    println("{}", occurrences(s, 'o'));
    println("{}", occurrences(s.as_slice(7), 'o'));
}
```

Output:

```text
12 world
2
1
```

## See also

- [str](str.md): the characters as a `std::basic_string`, a copy
- [slice](../../slice.md): a piece of the text that holds it
- [sgcl::mixin::text\<Derived, CharT, Traits\>](../text.md)

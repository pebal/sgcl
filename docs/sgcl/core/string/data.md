[sgcl](../../README.md) › [core](../README.md) › [string](README.md)

# sgcl::string::data

```cpp
const CharT* data() const noexcept;
```

Returns a pointer to the characters, which are followed by a terminator, `CharT()`: `data()[size()]` is the
terminator. The characters lie in the string's object, after its length and hash. The empty string, which has no
object, returns a pointer to a terminator of its own, never null.

The pointer is valid while some string holds the object: a copy of the string, a slice of it, a managed object that
holds it. A pointer taken from a temporary string dangles once the statement ends, as one taken from a
`std::string` does; a [slice](as_slice.md) of the string does not, it holds the object.

## Parameters

None.

## Return value

A pointer to the first character, never null.

## Complexity

Constant.

## Exceptions

None.

## Notes

The characters may hold a NUL of their own: a string made of a view or of `(s, n)` keeps every character it was
given, and a function that reads `data()` up to its first NUL sees only the text before it. [size](size.md) is the
number of characters.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <cstring>

using namespace sgcl;

int main() {
    string name = "alice";
    const char* chars = name.data();
    println("{} {} {}", std::strlen(chars), chars[0], chars[name.size()] == '\0');

    string empty;
    println("{} {}", empty.data() != nullptr, *empty.data() == '\0');

    string with_nul("ab\0cd", 5);
    println("{} {}", with_nul.size(), std::strlen(with_nul.data()));
}
```

Output:

```text
5 a true
true true
5 2
```

## See also

- [c_str](c_str.md): the same pointer, for an interface that takes a C string
- [as_slice, operator slice_type](as_slice.md): the characters as a slice that holds the object
- [size](size.md): the number of characters
- [sgcl::string](README.md)

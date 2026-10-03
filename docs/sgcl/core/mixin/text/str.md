[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [text](README.md)

# sgcl::mixin::text\<Derived, CharT, Traits\>::str

```cpp
std::basic_string<CharT, Traits> str() const noexcept;
```

Returns a `std::basic_string` with the same characters: for the interfaces that want one, and for building a new
string by appending.

## Parameters

None.

## Return value

A `std::basic_string<CharT, Traits>` holding a copy of the characters.

## Complexity

Linear in `size()`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string>

using namespace sgcl;

int main() {
    string s = "hello, world";
    std::string copy = s.str();
    copy += '!';
    println("{} {}", copy, s);
    println("{}", s.as_slice(7).str().size());
}
```

Output:

```text
hello, world! hello, world
5
```

## See also

- [view, operator view_type](view.md): the characters as a view, nothing copied
- [copy](copy.md): copies characters into an array
- [sgcl::mixin::text\<Derived, CharT, Traits\>](README.md)

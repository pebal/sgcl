[sgcl](../../README.md) › [core](../README.md) › [utf8](../utf8.md) › [encoded](../utf8-encoded.md)

# sgcl::utf8::encoded::view, operator std::string_view

```cpp
/*(1)*/ constexpr std::string_view view() const noexcept;
/*(2)*/ constexpr operator std::string_view() const noexcept;
```

Returns the bytes of the encoding as a `std::string_view` of `size` bytes over `bytes`. (2) is the same, implicit,
so an `encoded` is passed where a view is taken: a search, a `concat`, a comparison.

## Parameters

None.

## Return value

A view of the bytes of the encoding.

## Complexity

Constant.

## Exceptions

None.

## Notes

The view points into the value: it is valid while the value lives, and a view of a temporary `encoded` is valid
until the end of the full expression.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string s = "a·b·c";
    utf8::encoded dot(U'·');
    println("{} {}", s.find(dot.view()), s.rfind(dot));
    println("{}", string::concat("x", dot, "y"));
}
```

Output:

```text
1 4
x·y
```

## See also

- [(constructor)](utf8-encoded.md): encodes a code point
- [sgcl::utf8::encoded](../utf8-encoded.md)

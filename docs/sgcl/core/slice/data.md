[sgcl](../../README.md) › [core](../README.md) › [slice](../slice.md)

# sgcl::slice\<T\>::data

```cpp
T* data() const noexcept;
```

The first element as a plain pointer, null for a default-constructed slice. The elements are not terminated: a text
slice is a range of characters, not a C string; `str()` makes a `std::string` of them, `string(s)` a string.

## Parameters

None.

## Return value

A pointer to the first element.

## Complexity

Constant.

## Exceptions

None.

## Notes

The pointer keeps nothing alive: it is valid while the slice, or another holder of the owner, lives.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <cstdio>

using namespace sgcl;

int main() {
    string text = "name=value";
    string_slice value = text.as_slice(5);
    std::fwrite(value.data(), 1, value.size(), stdout);  // a pointer and a length, no terminator
    std::fputc('\n', stdout);
}
```

Output:

```text
value
```

## See also

- [size](size.md): the number of elements
- [operator std::span](operator_conv.md): the elements as a `std::span`
- [sgcl::slice\<T\>](../slice.md)

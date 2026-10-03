[sgcl](../../README.md) › [core](../README.md) › [array](README.md)

# sgcl::array\<T, N\>::front

```cpp
constexpr reference front() noexcept;                // (1)
constexpr const_reference front() const noexcept;    // (2)
```

Returns a reference to the first element, `(*this)[0]`. `array<T, 0>` has no `front`: an array without elements
has no first one, and the call is an error at compile time.

## Parameters

None.

## Return value

A reference to the first element.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    array<string, 3> queue = {"first", "second", "third"};
    println("{}", queue.front());

    queue.front() = "zeroth";
    println("{}", queue);
}
```

Output:

```text
first
["zeroth", "second", "third"]
```

## See also

- [back](back.md): the last element
- [operator[]](operator_at.md): the element at a position
- [sgcl::array\<T, N\>](README.md)

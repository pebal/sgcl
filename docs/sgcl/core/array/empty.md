[sgcl](../../README.md) › [core](../README.md) › [array](../array.md)

# sgcl::array\<T, N\>::empty

```cpp
[[nodiscard]] constexpr bool empty() const noexcept;
```

Checks whether the array has no elements: `N == 0`. The answer is a constant of the type, `false` for every
`array<T, N>` with `N > 0` and `true` for `array<T, 0>`.

## Parameters

None.

## Return value

`true` for `array<T, 0>`, `false` otherwise.

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
    array<int, 3> three = {};
    array<int, 0> none;
    println("{} {}", three.empty(), none.empty());

    constexpr bool nothing = array<string, 0>().empty();
    println("{}", nothing);
}
```

Output:

```text
false true
true
```

## See also

- [size](size.md): the number of elements
- [sgcl::array\<T, N\>](../array.md)

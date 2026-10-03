[sgcl](../../README.md) › [core](../README.md) › [array](../array.md)

# sgcl::array\<T, N\>::back

```cpp
/*(1)*/ constexpr reference back() noexcept;
/*(2)*/ constexpr const_reference back() const noexcept;
```

Returns a reference to the last element, `(*this)[N - 1]`. `array<T, 0>` has no `back`: an array without elements
has no last one, and the call is an error at compile time.

## Parameters

None.

## Return value

A reference to the last element.

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
    array<int, 4> steps = {1, 2, 4, 8};
    println("{}", steps.back());

    steps.back() *= 2;
    println("{}", steps);

    constexpr array<char, 3> letters = {'a', 'b', 'c'};
    constexpr char last = letters.back();
    println("{}", last);
}
```

Output:

```text
8
[1, 2, 4, 16]
c
```

## See also

- [front](front.md): the first element
- [operator[]](operator_at.md): the element at a position
- [sgcl::array\<T, N\>](../array.md)

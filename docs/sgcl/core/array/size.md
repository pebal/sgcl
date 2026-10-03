[sgcl](../../README.md) › [core](../README.md) › [array](../array.md)

# sgcl::array\<T, N\>::size

```cpp
constexpr size_type size() const noexcept;
```

Returns the number of elements, `N`: a constant of the type, usable in a constant expression.

## Parameters

None.

## Return value

`N`.

## Complexity

Constant.

## Exceptions

None.

## Notes

`std::tuple_size<array<T, N>>` gives the same number from the type alone
([Specializations](../array.md#specializations)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    array<double, 4> weights = {0.1, 0.2, 0.3, 0.4};
    println("{}", weights.size());

    constexpr array<int, 3> sizes = {};
    array<char, sizes.size()> letters = {'a', 'b', 'c'};  // a size taken at compile time
    println("{} {}", letters.size(), std::tuple_size_v<array<char, 3>>);
}
```

Output:

```text
4
3 3
```

## See also

- [empty](empty.md): checks whether the array is empty
- [max_size](max_size.md): the largest number of elements
- [sgcl::array\<T, N\>](../array.md)

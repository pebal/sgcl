[sgcl](../../README.md) › [core](../README.md) › [array](../array.md)

# sgcl::get (sgcl::array)

```cpp
#include "sgcl/core/array.h"   // or "sgcl/core.h"

namespace sgcl {
    /*(1)*/ template<size_t I, class T, size_t N> constexpr T& get(array<T, N>& a) noexcept;
    /*(2)*/ template<size_t I, class T, size_t N> constexpr const T& get(const array<T, N>& a) noexcept;
    /*(3)*/ template<size_t I, class T, size_t N> constexpr T&& get(array<T, N>&& a) noexcept;
}
```

Returns the element at the position `I`, given at compile time: an `I` that is not less than `N` is an error at
compile time, by a `static_assert`.

1. A reference to the element.
2. A const reference to the element of a const array.
3. The element of an array that is going away, as an rvalue reference: it may be moved from.

With the specializations of `std::tuple_size` and `std::tuple_element`
([Specializations](../array.md#specializations)), `get` is what makes structured bindings of an array work:
`auto [x, y, z] = a`.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the array |

## Return value

A reference to the element at `I`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <utility>

using namespace sgcl;

int main() {
    array<int, 3> a = {1, 2, 3};
    get<1>(a) = 20;
    auto [x, y, z] = a;  // structured bindings
    println("{} {} {}", x, y, z);

    array<string, 2> words = {"kept", "taken"};
    string taken = get<1>(std::move(words));
    println("{} {}", taken, std::tuple_size_v<array<string, 2>>);
}
```

Output:

```text
1 20 3
taken 2
```

## See also

- [at](at.md): the element at a position given at run time, with bounds checking
- [sgcl::array\<T, N\>](../array.md)

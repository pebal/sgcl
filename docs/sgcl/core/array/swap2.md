[sgcl](../../README.md) › [core](../README.md) › [array](../array.md)

# sgcl::swap (sgcl::array)

```cpp
friend constexpr void swap(array& l, array& r) noexcept(noexcept(l.swap(r)));
```

Exchanges the elements of `l` and `r`: `l.swap(r)` ([swap](swap.md)). A hidden friend, found by the arguments'
type: `swap(a, b)` written without a namespace, and `std::ranges::swap(a, b)`.

`array<T, 0>` declares it too, and it does nothing.

## Parameters

| Parameter | Description |
|---|---|
| `l`, `r` | the arrays to exchange the elements of |

## Return value

None.

## Complexity

Linear in `N`.

## Exceptions

What the swap of two elements throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    array<int, 3> a = {1, 2, 3};
    array<int, 3> b = {7, 8, 9};
    swap(a, b);
    println("{} {}", a, b);

    std::ranges::swap(a, b);
    println("{} {}", a, b);
}
```

Output:

```text
[7, 8, 9] [1, 2, 3]
[1, 2, 3] [7, 8, 9]
```

## See also

- [swap](swap.md): the member form
- [sgcl::array\<T, N\>](../array.md)

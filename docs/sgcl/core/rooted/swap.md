[sgcl](../../README.md) › [core](../README.md) › [rooted](../rooted.md)

# sgcl::rooted\<T\>::swap

```cpp
/*(1)*/ void swap(rooted& o) noexcept;
/*(2)*/ template<class T>
        void swap(rooted<T>& a, rooted<T>& b) noexcept;
```

Exchanges the values of two `rooted`s; the cells stay with their `rooted`s.

1. The member: this value and that of `o`.
2. The free function in `sgcl`: `a.swap(b)`.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the `rooted` to exchange with |
| `a`, `b` | the `rooted`s to exchange |

## Return value

None.

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
    rooted<string> left(string("left"));
    rooted<string> right(string("right"));
    left.swap(right);
    println("{} {}", *left, *right);

    swap(left, right);
    println("{} {}", *left, *right);
}
```

Output:

```text
right left
left right
```

## See also

- [operator=](operator_assign.md): shares another `rooted`'s value
- [sgcl::rooted\<T\>](../rooted.md)

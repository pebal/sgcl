[sgcl](../../README.md) › [core](../README.md) › [tracked_ptr](../tracked_ptr.md)

# sgcl::tracked_ptr\<T\>::swap

```cpp
void swap(tracked_ptr& p) noexcept;
```

Exchanges the two pointers, through a temporary on the stack.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the pointer to exchange with |

## Return value

None.

## Complexity

Constant: three stores with the barrier.

## Exceptions

None.

## Notes

There is no free `swap` for `tracked_ptr`; `std::swap` works through the move operations.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    tracked_ptr a = make_tracked<int>(1);
    tracked_ptr b = make_tracked<int>(2);
    a.swap(b);
    println("{} {}", *a, *b);

    std::swap(a, b);
    println("{} {}", *a, *b);
}
```

Output:

```text
2 1
1 2
```

## See also

- [operator=](operator_assign.md): assigns the pointer
- [sgcl::tracked_ptr\<T\>](../tracked_ptr.md)

[sgcl](../../README.md) › [core](../README.md) › [unique_ptr](../unique_ptr.md)

# sgcl::unique_ptr\<T\>::get

```cpp
pointer get() const noexcept;
```

Returns the raw pointer: the `get` of `std::unique_ptr`.

## Parameters

None.

## Return value

The address of the owned object, null when there is none.

## Complexity

Constant.

## Exceptions

None.

## Notes

The raw pointer is valid while the `unique_ptr` owns the object.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Point {
    int x, y;
};

int main() {
    unique_ptr p = make_tracked<Point>(1, 2);
    Point* raw = p.get();  // valid while p owns the Point
    raw->x = 5;
    println("{} {}", p->x, unique_ptr<Point>().get() == nullptr);
}
```

Output:

```text
5 true
```

## See also

- [release](release.md): hands the raw pointer out and leaves the owner empty
- [operator\*, operator->](operator_deref.md): the object
- [sgcl::unique_ptr\<T\>](../unique_ptr.md)

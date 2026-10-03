[sgcl](../../README.md) › [core](../README.md) › [unique_ptr](../unique_ptr.md)

# sgcl::unique_ptr\<T\>::operator\*, operator-\>

```cpp
/*(1)*/ typename std::add_lvalue_reference<T>::type operator*() const;
/*(2)*/ pointer operator->() const noexcept;
```

Access the owned object: the operators of `std::unique_ptr`.

1. The object, by reference.
2. The address of the object, for a member access.

Neither is for `unique_ptr<void>`. The owner must hold an object.

## Parameters

None.

## Return value

1. `*get()`.
2. `get()`.

## Complexity

Constant.

## Exceptions

None.

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
    p->x = (*p).y;
    println("{} {}", p->x, p->y);
}
```

Output:

```text
2 2
```

## See also

- [get](get.md): the raw pointer
- [operator bool](operator_bool.md): checks whether there is an object
- [sgcl::unique_ptr\<T\>](../unique_ptr.md)

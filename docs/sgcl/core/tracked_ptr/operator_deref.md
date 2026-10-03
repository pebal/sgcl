[sgcl](../../README.md) › [core](../README.md) › [tracked_ptr](../tracked_ptr.md)

# sgcl::tracked_ptr\<T\>::operator\*, operator-\>

```cpp
template<class U = element_type, std::enable_if_t<!std::is_void_v<U>, int> = 0>
U& operator*() const noexcept;                                                     // (1)
template<class U = element_type, std::enable_if_t<!std::is_void_v<U>, int> = 0>
U* operator->() const noexcept;                                                    // (2)
```

Access the object the pointer points at.

1. The object, by reference.
2. The address of the object, for a member access.

Neither is there for `tracked_ptr<void>`. Debug builds assert that the pointer is not null.

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
    tracked_ptr p = make_tracked<Point>(1, 2);
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
- [operator bool](operator_bool.md): checks whether the pointer is not null
- [sgcl::tracked_ptr\<T\>](../tracked_ptr.md)

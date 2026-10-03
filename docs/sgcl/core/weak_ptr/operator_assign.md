[sgcl](../../README.md) › [core](../README.md) › [weak_ptr](../weak_ptr.md)

# sgcl::weak_ptr\<T\>::operator=

```cpp
weak_ptr& operator=(const weak_ptr&) noexcept = default;                                                                                   // (1)
weak_ptr& operator=(weak_ptr&&) noexcept = default;                                                                                        // (2)
template<class U, std::enable_if_t<std::is_same_v<std::remove_cv_t<U>, std::remove_cv_t<T>> && std::is_convertible_v<U*, T*>, int> = 0>
weak_ptr& operator=(const weak_ptr<U>& w) noexcept;                                                                                        // (3)
template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
weak_ptr& operator=(const tracked_ptr<U>& p) noexcept;                                                                                     // (4)
weak_ptr& operator=(std::nullptr_t) noexcept;                                                                                              // (5)
```

Replaces the cell this pointer refers to.

1. Shares the cell of the other `weak_ptr`.
2. The same as (1): the source keeps its cell.
3. Shares the cell of `w`, a `weak_ptr` to `T` without `const`. A `weak_ptr` to a derived class is not assigned to a
   `weak_ptr` of its base or to `weak_ptr<void>`: assign its locked pointer, `base = w.lock()`, through (4)
   ([(constructor)](weak_ptr.md), Notes).
4. A new cell holding the object of `p`; the copies that shared the old cell keep it. An empty `weak_ptr` when `p` is
   null.
5. Drops the cell: the pointer is empty, expired.

## Parameters

| Parameter | Description |
|---|---|
| `w` | the `weak_ptr` whose cell is shared |
| `p` | the pointer to the object to observe |

## Return value

`*this`.

## Complexity

Constant: (4) one managed allocation, the cell.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    tracked_ptr a = make_tracked<int>(1);
    tracked_ptr b = make_tracked<int>(2);
    weak_ptr w = a;
    weak_ptr copy = w;  // shares the cell of a
    w = b;              // a new cell: the copy still sees a
    println("{} {}", *w.lock(), *copy.lock());

    w = nullptr;
    println("{} {}", w.expired(), *copy.lock());
}
```

Output:

```text
2 1
true 1
```

## See also

- [(constructor)](weak_ptr.md): constructs the pointer
- [reset](reset.md): drops the cell
- [sgcl::weak_ptr\<T\>](../weak_ptr.md)

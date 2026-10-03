[sgcl](../../README.md) › [core](../README.md) › [weak_ptr](../weak_ptr.md)

# sgcl::weak_ptr\<T\>::swap

```cpp
void swap(weak_ptr& w) noexcept;                       // (1)
template<class T>
void swap(weak_ptr<T>& l, weak_ptr<T>& r) noexcept;    // (2)
```

Exchanges the cells of two pointers.

1. The member: this pointer's cell and that of `w`.
2. The free function in `sgcl`: `l.swap(r)`.

## Parameters

| Parameter | Description |
|---|---|
| `w` | the pointer to exchange with |
| `l`, `r` | the pointers to exchange |

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
    tracked_ptr number = make_tracked<int>(1);
    weak_ptr w = number;
    weak_ptr<int> empty;
    swap(w, empty);
    println("{} {}", w.expired(), *empty.lock());

    w.swap(empty);
    println("{} {}", *w.lock(), empty.expired());
}
```

Output:

```text
true 1
1 true
```

## See also

- [reset](reset.md): drops the cell
- [sgcl::weak_ptr\<T\>](../weak_ptr.md)

[sgcl](../../README.md) › [core](../README.md) › [root_ptr](../root_ptr.md)

# sgcl::root_ptr\<T\>::swap

```cpp
/*(1)*/ void swap(root_ptr& o) noexcept;
/*(2)*/ template<class T>
        void swap(root_ptr<T>& l, root_ptr<T>& r) noexcept;
```

Exchanges the pointers of two roots; the cells stay with their `root_ptr`s.

1. The member: this root's pointer and that of `o`.
2. The free function in `sgcl`: `l.swap(r)`.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the root to exchange with |
| `l`, `r` | the roots to exchange |

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
    root_ptr<int> a = make_tracked<int>(1);
    root_ptr<int> b = make_tracked<int>(2);
    a.swap(b);
    println("{} {}", *a, *b);

    swap(a, b);
    println("{} {}", *a, *b);
}
```

Output:

```text
2 1
1 2
```

## See also

- [operator=](operator_assign.md): stores another pointer in the cell
- [sgcl::root_ptr\<T\>](../root_ptr.md)

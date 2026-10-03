[sgcl](../../README.md) › [core](../README.md) › [list](../list.md)

# sgcl::list\<T\>::emplace_front

```cpp
template<class... A>
reference emplace_front(A&&... a) noexcept(std::is_nothrow_constructible_v<T, A...>);
```

Constructs an element at the beginning from the arguments `a`, forwarded to the constructor of `T`, in a node of
its own, and returns a reference to it. The element is constructed before the node is linked: an argument may refer
to an element of this list.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments of the constructor of `T` |

## Return value

A reference to the new element.

## Complexity

Constant.

## Exceptions

What the constructor of `T` throws; none when it is noexcept. If an exception is thrown, the list is as it was
before the call.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    list<tracked_ptr<int>> ptrs;
    for (int i : range(1000)) {
        ptrs.push_back(make_tracked<int>(i));
    }
    int& first = *ptrs.emplace_front(make_tracked<int>(-1));
    first -= 1;
    println("{} pointers, the first to {}", ptrs.size(), *ptrs.front());
}
```

Output:

```text
1001 pointers, the first to -2
```

## See also

- [push_front](push_front.md): inserts an element at the beginning
- [emplace_back](emplace_back.md): constructs an element in place at the end
- [sgcl::list\<T\>](../list.md)

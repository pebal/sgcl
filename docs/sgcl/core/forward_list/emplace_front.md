[sgcl](../../README.md) › [core](../README.md) › [forward_list](../forward_list.md)

# sgcl::forward_list\<T\>::emplace_front

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
    forward_list<tracked_ptr<int>> ptrs;
    for (int i : range(1000)) {
        ptrs.push_front(make_tracked<int>(i));
    }
    int& first = *ptrs.emplace_front(make_tracked<int>(-1));
    first -= 1;
    println("the first to {}, the second to {}", *ptrs.front(), **std::next(ptrs.begin()));

    ptrs.pop_front();  // the int behind first is unreachable now
    println("the first to {}", *ptrs.front());
}
```

Output:

```text
the first to -2, the second to 999
the first to 999
```

## See also

- [push_front](push_front.md): inserts an element at the beginning
- [emplace_after](emplace_after.md): constructs an element in place after a position
- [sgcl::forward_list\<T\>](../forward_list.md)

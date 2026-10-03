[sgcl](../../README.md) › [core](../README.md) › [forward_list](../forward_list.md)

# sgcl::forward_list\<T\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the list has no element: whether the sentinel links a node.

## Parameters

None.

## Return value

`true` when the list is empty, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Notes

There is no `size()`, as in `std::forward_list`: the list keeps no count. `std::ranges::distance(l)` counts the
nodes, in linear time.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <iterator>

using namespace sgcl;

int main() {
    forward_list<int> l;
    println("{}", l.empty());

    l.push_front(1);
    l.push_front(2);
    println("{} {}", l.empty(), std::ranges::distance(l));
}
```

Output:

```text
true
false 2
```

## See also

- [max_size](max_size.md): the largest number of elements
- [clear](clear.md): destroys every element
- [sgcl::forward_list\<T\>](../forward_list.md)

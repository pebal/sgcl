[sgcl](../../README.md) › [immutable](../README.md) › [list](README.md)

# sgcl::immutable::list\<T\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the list has no elements. An empty list holds no cell.

## Parameters

None.

## Return value

`true` when `size() == 0`, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::list<int> l = {1, 2};
    int popped = 0;
    while (!l.empty()) {
        l = l.pop_front();
        ++popped;
    }
    println("{} popped, empty {}", popped, l.empty());
}
```

Output:

```text
2 popped, empty true
```

## See also

- [size](size.md): the number of elements
- [sgcl::immutable::list\<T\>](README.md)

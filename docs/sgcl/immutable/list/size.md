[sgcl](../../README.md) › [immutable](../README.md) › [list](../list.md)

# sgcl::immutable::list\<T\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of elements: a word of the list, kept as cells are added and dropped, so nothing is counted.

## Parameters

None.

## Return value

The number of elements.

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
    immutable::list<int> l = {1, 2, 3};
    println("{} {} {}", l.size(), l.push_front(0).size(), l.pop_front().size());
}
```

Output:

```text
3 4 2
```

## See also

- [empty](empty.md): checks whether the list is empty
- [sgcl::immutable::list\<T\>](../list.md)

[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of elements in the deque.

## Parameters

None.

## Return value

The number of elements.

## Complexity

Constant: the count is a word of the deque object.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque<int> d;
    for (int i : range(5)) {
        d.push_back(i);
        d.push_front(-i);
    }
    println("{}", d.size());

    d.pop_front();
    println("{}", d.size());
}
```

Output:

```text
10
9
```

## See also

- [empty](empty.md): checks whether the deque is empty
- [max_size](max_size.md): the largest number of elements a deque may hold
- [resize](resize.md): changes the number of elements
- [sgcl::deque\<T\>](../deque.md)

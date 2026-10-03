[sgcl](../../README.md) › [concurrent](../README.md) › [priority_queue](README.md)

# sgcl::concurrent::priority_queue\<T, Compare\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the queue holds an element: a load of the count kept beside the heap, without the lock.

## Parameters

None.

## Return value

`true` when the count is zero, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Notes

The count is stored under the lock by every push and pop as it completes, and read here without it: the answer is
as of the last push or pop completed, and may be stale when it returns under concurrent pushes and pops. A
consumer takes an element with [try_pop](try_pop.md), whose answer is the element itself.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::priority_queue<int> numbers;
    println("{}", numbers.empty());

    numbers.push(3);
    println("{}", numbers.empty());
}
```

Output:

```text
true
false
```

## See also

- [size](size.md): the number of elements
- [sgcl::concurrent::priority_queue\<T, Compare\>](README.md)

[sgcl](../../README.md) › [concurrent](../README.md) › [queue](../queue.md)

# sgcl::concurrent::queue\<T\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the queue holds an element: a walk from the head to the first node whose element is not taken.

## Parameters

None.

## Return value

`true` when the walk found no element, `false` otherwise.

## Complexity

Constant, plus the walk over the taken nodes the head has not passed yet: a node or two.

## Exceptions

None.

## Notes

Under concurrent pushes and pops the answer is of the moment of the walk and may be stale when it returns: a
consumer takes an element with [try_pop](try_pop.md), whose answer is the element itself, rather than with
`empty` and then a pop.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::queue<int> numbers;
    println("{}", numbers.empty());

    numbers.push(7);
    println("{}", numbers.empty());

    numbers.try_pop();
    println("{}", numbers.empty());
}
```

Output:

```text
true
false
true
```

## See also

- [size](size.md): counts the elements
- [sgcl::concurrent::queue\<T\>](../queue.md)

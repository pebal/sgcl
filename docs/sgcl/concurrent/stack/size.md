[sgcl](../../README.md) › [concurrent](../README.md) › [stack](../stack.md)

# sgcl::concurrent::stack\<T\>::size

```cpp
size_type size() const noexcept;
```

Counts the elements: a walk over the nodes from the head down, as `std::forward_list` would count them.

## Parameters

None.

## Return value

The number of nodes the walk found.

## Complexity

Linear in the number of elements.

## Exceptions

None.

## Notes

The stack keeps no count, as Java's `ConcurrentLinkedDeque` keeps none: a count would be one more word every push
and every pop writes. The walk starts at the head it loads and follows the links below it, which no thread
changes once a node is published: the number is that of the stack at the moment of the load, and may be stale
when it returns under concurrent pushes and pops; it is exact once the other threads are quiet.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::stack<int> numbers;
    for (int i : range(5)) {
        numbers.push(i);
    }
    numbers.try_pop();
    println("{}", numbers.size());
}
```

Output:

```text
4
```

## See also

- [empty](empty.md): checks whether the stack holds an element, in one load
- [sgcl::concurrent::stack\<T\>](../stack.md)

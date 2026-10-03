[sgcl](../../README.md) › [concurrent](../README.md) › [queue](../queue.md)

# sgcl::concurrent::queue\<T\>::size

```cpp
size_type size() const noexcept;
```

Counts the elements: a walk over the nodes from the head to the last one, counting those whose element is not
taken.

## Parameters

None.

## Return value

The number of elements the walk found.

## Complexity

Linear in the number of elements.

## Exceptions

None.

## Notes

The queue keeps no count, as Java's `ConcurrentLinkedQueue` keeps none: a count would be one more word every push
and every pop writes. Under concurrent pushes and pops the walk passes the nodes at different moments, so the
number is a snapshot of no particular moment; it is exact once the other threads are quiet.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::queue<int> numbers;
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

- [empty](empty.md): checks whether the queue holds an element, without the count
- [sgcl::concurrent::queue\<T\>](../queue.md)

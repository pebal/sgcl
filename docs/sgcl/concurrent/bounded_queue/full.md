[sgcl](../../README.md) › [concurrent](../README.md) › [bounded_queue](README.md)

# sgcl::concurrent::bounded_queue\<T\>::full

```cpp
bool full() const noexcept;
```

Checks whether the queue holds as many elements as it has cells: [size](size.md) against
[capacity](capacity.md).

## Parameters

None.

## Return value

`true` when the size was the capacity, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Notes

Under concurrent pushes and pops the answer is of the moment of the two loads of `size` and may be stale when it
returns: a producer appends with [try_push](try_push.md), whose `false` is the answer, rather than with `full`
and then a push.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::bounded_queue<int> numbers(2);
    numbers.push(1);
    println("{}", numbers.full());

    numbers.push(2);
    println("{}", numbers.full());
}
```

Output:

```text
false
true
```

## See also

- [empty](empty.md): checks whether the queue holds an element
- [try_push](try_push.md): appends an element, or returns `false` when the queue is full
- [sgcl::concurrent::bounded_queue\<T\>](README.md)

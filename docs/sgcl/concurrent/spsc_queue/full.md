[sgcl](../../README.md) › [concurrent](../README.md) › [spsc_queue](../spsc_queue.md)

# sgcl::concurrent::spsc_queue\<T\>::full

```cpp
bool full() const noexcept;
```

Either side's, or any thread's. Checks whether the queue holds as many elements as it has cells: [size](size.md)
against [capacity](capacity.md).

## Parameters

None.

## Return value

`true` when the size was the capacity, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Notes

While the consumer pops, the answer may be stale when it returns: the producer appends with
[try_push](try_push.md), whose `false` is the answer, rather than with `full` and then a push.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::spsc_queue<int> numbers(2);
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
- [sgcl::concurrent::spsc_queue\<T\>](../spsc_queue.md)

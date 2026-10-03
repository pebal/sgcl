[sgcl](../../README.md) › [concurrent](../README.md) › [spsc_queue](../spsc_queue.md)

# sgcl::concurrent::spsc_queue\<T\>::empty

```cpp
bool empty() const noexcept;
```

Either side's, or any thread's. Checks whether the queue holds an element: [size](size.md) against zero.

## Parameters

None.

## Return value

`true` when the size was zero, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Notes

While the producer pushes, the answer may be stale when it returns: the consumer takes an element with
[try_pop](try_pop.md), whose answer is the element itself, rather than with `empty` and then a pop.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::spsc_queue<int> numbers(4);
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

- [full](full.md): checks whether the queue holds as many elements as it has cells
- [size](size.md): the number of elements
- [sgcl::concurrent::spsc_queue\<T\>](../spsc_queue.md)

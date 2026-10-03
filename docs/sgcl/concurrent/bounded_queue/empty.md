[sgcl](../../README.md) › [concurrent](../README.md) › [bounded_queue](../bounded_queue.md)

# sgcl::concurrent::bounded_queue\<T\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the queue holds an element: [size](size.md) against zero.

## Parameters

None.

## Return value

`true` when the size was zero, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Notes

Under concurrent pushes and pops the answer is of the moment of the two loads of `size` and may be stale when it
returns: a consumer takes an element with [try_pop](try_pop.md), whose answer is the element itself, rather than
with `empty` and then a pop.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::bounded_queue<int> numbers(4);
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
- [sgcl::concurrent::bounded_queue\<T\>](../bounded_queue.md)

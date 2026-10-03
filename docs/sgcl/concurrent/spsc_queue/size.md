[sgcl](../../README.md) › [concurrent](../README.md) › [spsc_queue](README.md)

# sgcl::concurrent::spsc_queue\<T\>::size

```cpp
size_type size() const noexcept;
```

Either side's, or any thread's. Returns the number of elements: the tail less the head, two loads, at most the
capacity.

## Parameters

None.

## Return value

The number of elements, from `0` to [capacity](capacity.md).

## Complexity

Constant.

## Exceptions

None.

## Notes

On the producer's thread the tail is its own and exact, and the head is the consumer's as of its last store; on
the consumer's thread the other way round. Under a running other side the number is a snapshot of some moment,
and on any third thread a snapshot of no particular moment; it is exact once the other side is quiet.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::spsc_queue<int> numbers(8);
    for (int i : range(5)) {
        numbers.push(i);
    }
    numbers.try_pop();
    println("{} of {}", numbers.size(), numbers.capacity());
}
```

Output:

```text
4 of 8
```

## See also

- [empty](empty.md), [full](full.md): the size against zero and the capacity
- [capacity](capacity.md): the number of cells
- [sgcl::concurrent::spsc_queue\<T\>](README.md)

[sgcl](../../README.md) › [concurrent](../README.md) › [bounded_queue](../bounded_queue.md)

# sgcl::concurrent::bounded_queue\<T\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of elements: the enqueue position less the dequeue one, two loads, at most the capacity.

## Parameters

None.

## Return value

The number of elements, from `0` to [capacity](capacity.md).

## Complexity

Constant.

## Exceptions

None.

## Notes

The two positions are loaded one after the other, so under concurrent pushes and pops the number is a snapshot of
no particular moment; it is exact once the other threads are quiet. A cell reserved by a push or a pop in
progress is counted with the side that reserved it: a cell a producer has won counts before its element is in,
and a cell a consumer has won no longer counts while its element is moved out. A cell published without an
element, by a push whose constructor threw, counts until a consumer has passed it.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::bounded_queue<int> numbers(8);
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
- [sgcl::concurrent::bounded_queue\<T\>](../bounded_queue.md)

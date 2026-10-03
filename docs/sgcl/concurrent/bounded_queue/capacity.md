[sgcl](../../README.md) › [concurrent](../README.md) › [bounded_queue](../bounded_queue.md)

# sgcl::concurrent::bounded_queue\<T\>::capacity

```cpp
size_type capacity() const noexcept;
```

Returns the number of cells of the ring: the capacity given to the [constructor](bounded_queue.md), rounded up to
a power of two, at least two.

## Parameters

None.

## Return value

The number of elements the queue holds at most.

## Complexity

Constant.

## Exceptions

None.

## Notes

The capacity is fixed at construction, and no thread writes it after: it may be read from any thread at any time
and is never stale.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::bounded_queue<int> small(3);
    concurrent::bounded_queue<int> large(1000);
    println("{} {}", small.capacity(), large.capacity());
}
```

Output:

```text
4 1024
```

## See also

- [size](size.md): the number of elements
- [full](full.md): checks whether the queue holds as many elements as it has cells
- [sgcl::concurrent::bounded_queue\<T\>](../bounded_queue.md)

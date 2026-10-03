[sgcl](../../README.md) › [concurrent](../README.md) › [spsc_queue](README.md)

# sgcl::concurrent::spsc_queue\<T\>::capacity

```cpp
size_type capacity() const noexcept;
```

Either side's, or any thread's. Returns the number of cells of the ring: the capacity given to the
[constructor](spsc_queue.md), rounded up to a power of two, at least one.

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
    concurrent::spsc_queue<int> small(3);
    concurrent::spsc_queue<int> large(1000);
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
- [sgcl::concurrent::spsc_queue\<T\>](README.md)

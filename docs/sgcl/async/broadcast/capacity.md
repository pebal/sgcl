[sgcl](../../README.md) › [async](../README.md) › [broadcast](../broadcast.md)

# sgcl::async::broadcast\<T\>::capacity

```cpp
size_type capacity() const noexcept;
```

The number of values the ring holds: the capacity given to the constructor, rounded up to a power of two, at least
1. A subscription more than this many values behind the sender loses the oldest ones.

## Parameters

None.

## Return value

The number of slots of the ring.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::broadcast<int> events(3);
    auto s = events.subscribe();
    for (int i : {1, 2, 3, 4, 5, 6}) {
        events.send(i);
    }
    println("capacity {}", events.capacity());
    auto oldest = s.try_receive();
    println("{}, {} lost before it", oldest, s.lagged());
}
```

Output:

```text
capacity 4
3, 2 lost before it
```

## See also

- [(constructor)](broadcast.md): where the capacity is given
- [lagged](../broadcast-subscription/lagged.md): the values a subscription lost
- [sgcl::async::broadcast\<T\>](../broadcast.md)

[sgcl](../../README.md) › [async](../README.md) › [broadcast](../broadcast/README.md) › [subscription](README.md)

# sgcl::async::broadcast\<T\>::subscription::lagged

```cpp
size_t lagged() const noexcept;
```

The number of values lost before the last one received: the values the ring lapped this subscription by, since the
receive before it. tokio's `Lagged(n)`, as a count beside the value rather than an error in its place, so that a loop
over the values reads it after a receive when it cares, and goes on with the value either way. Each receive sets it
anew; a value received with nothing lost before it makes it 0.

## Parameters

None.

## Return value

The number of values lost between the receive before the last one and the last one; 0 before the first receive.

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
    async::broadcast<int> ticks(2);
    auto s = ticks.subscribe();
    for (int i : {1, 2, 3, 4, 5}) {
        ticks.send(i);
    }

    auto first = s.try_receive();
    println("{}, {} lost before it", first, s.lagged());
    auto second = s.try_receive();
    println("{}, {} lost before it", second, s.lagged());
}
```

Output:

```text
4, 3 lost before it
5, 0 lost before it
```

## See also

- [capacity](../broadcast/capacity.md): how far a subscription may fall behind
- [receive](receive.md): the receive whose count this is
- [sgcl::async::broadcast\<T\>::subscription](README.md)

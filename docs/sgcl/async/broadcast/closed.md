[sgcl](../../README.md) › [async](../README.md) › [broadcast](README.md)

# sgcl::async::broadcast\<T\>::closed

```cpp
bool closed() const noexcept;
```

Checks whether the broadcast is closed. The subscriptions of a closed broadcast may still have values to receive.

## Parameters

None.

## Return value

`true` once [close](close.md) was called, `false` before.

## Complexity

Constant: one atomic load.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::broadcast<int> events(4);
    auto s = events.subscribe();
    events.send(1);
    println("{}", events.closed());
    events.close();
    println("{} {}", events.closed(), s.try_receive());
}
```

Output:

```text
false
true 1
```

## See also

- [close](close.md): closes the broadcast
- [closed](../broadcast-subscription/closed.md): the same question asked of a subscription
- [sgcl::async::broadcast\<T\>](README.md)

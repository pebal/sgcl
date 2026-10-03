[sgcl](../../README.md) › [async](../README.md) › [broadcast](../broadcast.md) › [subscription](../broadcast-subscription.md)

# sgcl::async::broadcast\<T\>::subscription::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the subscription is of a broadcast: made by [subscribe](../broadcast/subscribe.md) and not moved
from. A default-constructed or a moved-from subscription is empty, and is only assigned to or destroyed.

## Parameters

None.

## Return value

`true` for a subscription of a broadcast, `false` for an empty one.

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
    async::broadcast<int> events(4);
    async::broadcast<int>::subscription s;
    println("{}", (bool)s);
    s = events.subscribe();
    println("{}", (bool)s);
}
```

Output:

```text
false
true
```

## See also

- [(constructor)](broadcast-subscription.md): the empty subscription
- [sgcl::async::broadcast\<T\>::subscription](../broadcast-subscription.md)

[sgcl](../../README.md) › [async](../README.md) › [broadcast](../broadcast/README.md) › [subscription](README.md)

# sgcl::async::broadcast\<T\>::subscription::operator=

```cpp
subscription& operator=(subscription&& other) noexcept;    // (1)
subscription& operator=(const subscription&) = delete;     // (2)
```

1. Ends this subscription as its destructor does, counting it off the values it has not passed, then takes `other`
   over: its broadcast, its cursor, its record and the count of the values it lost. `other` is left empty. An
   assignment of a subscription to itself does nothing.
2. A subscription is not copyable.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the subscription to take over |

## Return value

`*this`.

## Complexity

Linear in the number of values this subscription has not passed, at most the capacity of its ring; constant for an
empty one.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::broadcast<int> events(4);
    async::broadcast<int>::subscription current;
    current = events.subscribe();
    events.send(1);

    current = events.subscribe();  // the first one counted off: 1 is let go of
    events.send(2);
    println("{} {}", events.subscribers(), current.try_receive());
}
```

Output:

```text
1 2
```

## See also

- [(constructor)](broadcast-subscription.md): makes an empty subscription, or takes one over
- [sgcl::async::broadcast\<T\>::subscription](README.md)

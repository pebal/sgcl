[sgcl](../../README.md) › [async](../README.md) › [broadcast](../broadcast.md) › [subscription](../broadcast-subscription.md)

# sgcl::async::broadcast\<T\>::subscription::subscription

```cpp
/*(1)*/ subscription() noexcept;
/*(2)*/ subscription(subscription&& other) noexcept;
/*(3)*/ subscription(const subscription&) = delete;
```

1. An empty subscription, of no broadcast: one to assign a subscription to later. A subscription of a broadcast is
   made by [subscribe](../broadcast/subscribe.md).
2. Takes `other` over: its broadcast, its cursor, its record and the count of the values it lost. `other` is left
   empty. The count of the broadcast's subscribers does not change.
3. A subscription is not copyable: a cursor has one reader.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the subscription to take over |

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
    async::broadcast<int>::subscription none;
    println("{}", (bool)none);

    auto s = events.subscribe();
    events.send(1);
    async::broadcast<int>::subscription taken = std::move(s);
    println("{} {} {}", (bool)s, (bool)taken, events.subscribers());
    println("{}", taken.try_receive());
}
```

Output:

```text
false
false true 1
1
```

## See also

- [subscribe](../broadcast/subscribe.md): a subscription of a broadcast
- [operator=](operator_assign.md): takes another subscription over
- [sgcl::async::broadcast\<T\>::subscription](../broadcast-subscription.md)

[sgcl](../../README.md) › [async](../README.md) › [broadcast](../broadcast.md)

# sgcl::async::broadcast\<T\>::send

```cpp
bool send(const T& value);    // (1)
bool send(T&& value);         // (2)
```

Sends a value to every subscription alive, without waiting. The value goes into a node of its own, the node into the
next slot of the ring, and the subscriptions waiting for it are woken. A subscription that has not read the value
the slot held a lap before loses it, and its next receive counts it in [lagged](../broadcast-subscription/lagged.md).
A value nobody subscribes to is dropped.

1. Sends a copy of `value`.
2. Sends `value`, moved.

Many threads and tasks may send at once; each subscription sees the values in the order their positions were taken.
A send never waits for a subscriber. It waits only when the position a lap before is not committed yet, for a
sender between its reservation and its store: a few instructions.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value to send |

## Return value

`true` when the value was sent; `false` when the broadcast is closed, and the value is not sent.

## Complexity

Linear in the number of subscriptions: the node allocated, an atomic add, a store and the commit, then a walk over
the subscriptions, a load each, to wake the ones that wait.

## Exceptions

- `std::system_error` when the send wakes a waiting task and the wake starts the scheduler's workers, one of which
  cannot be started.
- What the copy (1) or the move constructor of `T` throws, before anything is sent.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::broadcast<int> events(4);
    println("{}", events.send(1));  // nobody subscribes: dropped

    auto s = events.subscribe();
    events.send(2);
    events.close();
    println("{}", events.send(3));
    println("{}", s.receive().wait());
    println("{}", s.receive().wait());
}
```

Output:

```text
true
false
2
nullopt
```

## See also

- [subscribe](subscribe.md): the receivers
- [close](close.md): no more sends
- [sgcl::async::broadcast\<T\>](../broadcast.md)

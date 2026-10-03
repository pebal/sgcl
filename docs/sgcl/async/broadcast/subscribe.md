[sgcl](../../README.md) › [async](../README.md) › [broadcast](../broadcast.md)

# sgcl::async::broadcast\<T\>::subscribe

```cpp
subscription subscribe();
```

Makes a [subscription](../broadcast-subscription.md): a receiver with a cursor of its own into the ring, starting at
the next value sent. The subscription counts itself in with the same atomic word a send takes its position from, so
it receives every value sent after the call, unless the ring laps it, and none sent before.

## Parameters

None.

## Return value

The subscription, counted among the [subscribers](subscribers.md) until it is destroyed or assigned over.

## Complexity

Constant: a compare-exchange on the broadcast's word, and the subscription's record, one managed object, linked
into the list the senders walk.

## Exceptions

`length_error` when 4095 subscriptions are alive already.

## Notes

A subscription made after the close receives nothing: its receives give nothing at once.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::broadcast<string> news(8);
    auto early = news.subscribe();
    news.send("first");
    auto late = news.subscribe();  // from the next value on
    news.send("second");
    news.close();

    while (auto n = early.receive().wait()) {
        println("early: {}", *n);
    }
    while (auto n = late.receive().wait()) {
        println("late: {}", *n);
    }
}
```

Output:

```text
early: first
early: second
late: second
```

## See also

- [subscribers](subscribers.md): the number of subscriptions alive
- [subscription](../broadcast-subscription.md): what is received through it
- [sgcl::async::broadcast\<T\>](../broadcast.md)

[sgcl](../../README.md) › [async](../README.md) › [broadcast](../broadcast.md)

# sgcl::async::broadcast\<T\>::close

```cpp
void close();
```

Ends every subscription: no more sends. Every subscription still receives what was sent before the close, then
nothing: a receive gives `nullopt` at once, a waiting one is woken with it, and a select case is served at once. A
send after the close gives `false`. A second `close()` does nothing.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of subscriptions, each looked at and the waiting ones woken.

## Exceptions

`std::system_error` when the close wakes a waiting task and the wake starts the scheduler's workers, one of which
cannot be started.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> count(async::broadcast<int>::subscription events) {
    int n = 0;
    while (co_await events.receive()) {
        ++n;
    }
    co_return n;  // closed and drained
}

int main() {
    async::broadcast<int> events(8);
    async::task<int> counting = async::spawn(count(events.subscribe()));
    events.send(1);
    events.send(2);
    events.close();
    println("{} received", counting.wait());
    println("{}", events.closed());
}
```

Output:

```text
2 received
true
```

## See also

- [closed](closed.md): checks whether the broadcast is closed
- [receive](../broadcast-subscription/receive.md): what a subscription gives after the close
- [sgcl::async::broadcast\<T\>](../broadcast.md)

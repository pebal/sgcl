[sgcl](../../README.md) › [async](../README.md) › [broadcast](../broadcast.md) › [subscription](../broadcast-subscription.md)

# sgcl::async::broadcast\<T\>::subscription::on_receive

```cpp
template<class F>
auto on_receive(F f);
```

A receive as a case of a [select](../select.md): the case is served when a value is there for this subscription, or
when the broadcast is closed, and its body `f` is called then, after the wait, with the value. A body that takes `T`
is called only with a value; one that takes `optional<T>` (one callable with it) is also called with `nullopt` when
the broadcast is closed and drained. The value is taken from the ring when the case is served, not when it is made:
another case of the select may win, and then nothing is taken.

When a value is there or the broadcast is closed, the case is served at once. Otherwise it waits on a round: a
channel of signals closed by the commit of the position the cursor is at, shared by the cases of every subscription
waiting for that position.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the body of the case: `void(T)` or `void(optional<T>)` |

## Return value

The case, a `receive_case` over the body, to be passed to [select](../select.md) by value. Nothing is received until
the select is carried out, and a case is for one select.

## Complexity

Constant: at most one managed object, the round of the position, when no case waits for it yet.

## Exceptions

- `std::system_error` when the case replaces a round of a position committed already, whose close wakes a waiting
  task, and the wake starts the scheduler's workers, one of which cannot be started.
- What the move constructor of `F` throws.

When the case is served, what the body throws, and what the copy of `T` throws, comes out of the select.

## Notes

The subscription must not be empty, and is read by one thread or task at a time: one case of it per select.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> listen(async::broadcast<int>::subscription events, async::channel<void> quit) {
    int total = 0;
    for (bool on = true; on;) {
        co_await async::select(
            events.on_receive([&](optional<int> v) {
                if (v) {
                    total += *v;
                } else {
                    on = false;  // closed and drained
                }
            }),
            quit.on_receive([&] { on = false; }));
    }
    co_return total;
}

int main() {
    async::broadcast<int> events(8);
    async::channel<void> quit;
    async::task<int> listening = async::spawn(listen(events.subscribe(), quit));
    for (int i : range(1, 5)) {
        events.send(i);
    }
    events.close();
    println("{}", listening.wait());
}
```

Output:

```text
10
```

## See also

- [receive](receive.md): the receive alone
- [select](../select.md): the wait over the cases
- [on_receive](../channel/on_receive.md): the same case on a channel
- [sgcl::async::broadcast\<T\>::subscription](../broadcast-subscription.md)

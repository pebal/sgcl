[sgcl](../README.md) › [async](README.md) › [broadcast](broadcast.md)

# sgcl::async::broadcast\<T\>::subscription

```cpp
#include "sgcl/async/broadcast.h"   // or "sgcl/async.h"

namespace sgcl::async {
    template<class T>
    class broadcast {
    public:
        class subscription;
    };
}
```

`sgcl::async::broadcast<T>::subscription` is the receiving side of a [broadcast](broadcast.md): a cursor of its own
into the broadcast's ring, made by [subscribe](broadcast/subscribe.md) at the position of the next value sent. Each
receive copies the value at the cursor out of the ring and moves the cursor on; a subscription that falls more than
the ring's capacity behind is lapped, its next receive gives the oldest value still there, and
[lagged](broadcast-subscription/lagged.md) says how many it lost before that one. It is received from the three ways
of the module: a thread writes `s.receive().wait()` and blocks, a task writes `co_await s.receive()` and holds no
thread, a [select](select.md) takes `s.on_receive(f)` as a case. Once the broadcast is closed, a subscription
receives what is left past its cursor, then nothing.

A subscription holds the broadcast's ring, which lives on while a subscription holds it, and a record of its wait,
one managed object made with it: the position it waits for, its task's frame or its thread's park word, and a link
in the list the senders walk. A sender that commits the position wakes the one subscriber waiting for it, once;
nothing is allocated per wait. Destroyed, the subscription counts itself off the values it has not passed, so that
the ring lets them go once every other subscription has passed them too.

## Rules

- A subscription holds tracked pointers, so it lives where a `tracked_ptr` may: a task's parameter or local, a
  stack, a member of a managed object ([The rules](../core/README.md#the-rules), 1).
- Move-only. A default-constructed or a moved-from subscription is empty: it is only assigned to, destroyed or asked
  whether it is empty ([operator bool](broadcast-subscription/operator_bool.md)).
- A subscription is read by one thread or task at a time: the cursor is its own. Any number of subscriptions of one
  broadcast are read at once.
- A task that awaits a receive must have a managed frame ([README: Coroutines](README.md#coroutines)).
- A subscription destroyed or assigned over counts itself off the values still in the ring past its cursor: at most
  the capacity, whatever its lag. A subscription nobody reads holds no value past a lap of the ring.

## Member types

| Type | Definition |
|---|---|
| `receive_op` | the awaiter of a receive in a task ([receive](broadcast-subscription/receive.md)) |
| `receive_case<F>` | a receive case of a select ([on_receive](broadcast-subscription/on_receive.md)) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](broadcast-subscription/broadcast-subscription.md) | makes an empty subscription, or takes one over |
| `(destructor)` | counts the subscription off the values it has not passed |
| [operator=](broadcast-subscription/operator_assign.md) | takes another subscription over |

#### Operations

| Function | Description |
|---|---|
| [receive](broadcast-subscription/receive.md) | receives the next value, waiting for one |
| [try_receive](broadcast-subscription/try_receive.md) | receives the next value if one is there |

#### Select cases

| Function | Description |
|---|---|
| [on_receive](broadcast-subscription/on_receive.md) | a case of a select served by a value or by the close |

#### Observers

| Function | Description |
|---|---|
| [lagged](broadcast-subscription/lagged.md) | the number of values lost before the last one received |
| [closed](broadcast-subscription/closed.md) | checks whether the broadcast is closed |
| [operator bool](broadcast-subscription/operator_bool.md) | checks whether the subscription is of a broadcast |

## Complexity

A receive that finds its value: constant, the copy of the value and an atomic decrement on its node. A receive that
waits: a thread looks for the next commit for a few microseconds, then parks; a task registers and suspends. Nothing
is allocated per receive.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::broadcast<int> ticks(4);
    async::broadcast<int>::subscription slow = ticks.subscribe();
    for (int i : range(1, 11)) {
        ticks.send(i);  // nobody reads yet: the ring keeps the last four
    }
    ticks.close();

    while (auto t = slow.receive().wait()) {
        if (slow.lagged() > 0) {
            println("{} lost", slow.lagged());
        }
        println("{}", *t);
    }
}
```

Output:

```text
6 lost
7
8
9
10
```

## See also

- [broadcast](broadcast.md): the sending side
- [subscribe](broadcast/subscribe.md): makes a subscription
- [select](select.md): a receive as a case
- [channel](channel.md): one receiver per element

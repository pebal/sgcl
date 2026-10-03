[sgcl](../../README.md) › [async](../README.md)

# sgcl::async::broadcast\<T\>

```cpp
#include "sgcl/async/broadcast.h"   // or "sgcl/async.h"

namespace sgcl::async {
    template<class T>
    class broadcast;
}
```

`sgcl::async::broadcast<T>` is a channel every subscriber receives every value from: tokio's `broadcast`, Kotlin's
`SharedFlow`, the event bus of a user interface. A [channel](../channel/README.md) hands each element to one receiver; here
`b.send(v)` goes to every [subscription](../broadcast-subscription/README.md) alive, each reading at its own pace from one ring
shared by all: the sender's position and a cursor per subscription. A subscription made later starts at the position
of its making and sees what is sent from then on. The ring holds `capacity` values, rounded up to a power of two: a
subscription that falls further behind loses the oldest, its next receive is the oldest still there, and
[lagged()](../broadcast-subscription/lagged.md) says how many were lost before it (tokio's `Lagged(n)`, as a count
beside the value rather than an error in its place). A value stays in the ring until every subscription alive at its
send has passed it, or until it is lapped, and is let go of then: a `tracked_ptr` value is held exactly that long,
and a subscription dropped counts itself off the values it has not passed. [close](close.md) ends every
subscription: what was sent is still received, then nothing. A subscription is received from the three ways of the
module: a thread blocks on `s.receive().wait()`, a task writes `co_await s.receive()` and holds no thread, a
[select](../select.md) takes `s.on_receive(f)` as a case; a send never waits.

How it is made: one word holds the positions reserved so far and the number of subscriptions together, so that a
send takes the next position and the count of the subscriptions that will read it in one atomic add, and a
subscription takes its cursor and counts itself in with another: the count on a value is exact, and the value's node
lets the value go when the count reaches zero. The slots of the ring hold the values' nodes by atomic tracked
pointers: a reader loads the node, copies the value out of it and counts itself off, and a sender overwriting the
slot meanwhile frees nothing (the node is the collector's when nothing holds it), which is what lets readers read a
slot no lock protects ([README: Lock-free containers](../../concurrent/README.md#lock-free-containers)). Senders publish
in order: a second word counts the positions committed, advanced by whichever sender finds the next slot stored (a
sender that stored ahead of a slower one leaves its commit to that one), so that a reader waits for a position by its
commit alone.

The wait of a receive is kept with the subscription (tokio's shape, the waker in the receiver): the position it waits
for, its task's frame or its thread's park word, and a link in the list of subscriptions the senders walk. A receive
that finds nothing registers the position and looks once more; the sender that commits past it walks the
subscriptions, claims each registration behind the commit with a compare-exchange and wakes that subscriber once, a
task handed to the scheduler, a thread through its word; nothing is allocated and no list shared by every waiter is
pushed on per value. The tasks one walk wakes go to the scheduler's queues together, with one worker woken for them.
A thread looks for the next commit for a few microseconds before it parks, as the queues of the concurrent module
do, since a sender at work is nanoseconds from it. A select case keeps a round of its own, a channel of signals
closed by the commit of the position it waits for, since a case must be served with a value and a registration kept
with a subscription could be a stale one.

## Rules

- A `broadcast` holds its ring by a `tracked_ptr`, so it lives where one may: on a thread's stack or inside a managed
  object ([The rules](../../core/README.md#the-rules), 1); it is neither copyable nor movable. A subscription is movable,
  not copyable, and lives in the same places (a task's parameter or local, a member of a managed object); the ring
  outlives the `broadcast` object while a subscription holds it.
- `T` is copied to every subscription.
- A send never waits and never fails but for the close: a value nobody subscribes to is dropped, and a subscription
  that does not read is lapped, never a brake on the sender (Go's channel and tokio's `mpsc` apply back-pressure,
  tokio's `broadcast` does not).
- Each subscription is read by one thread or task at a time, its cursor being its own; many threads and tasks may
  send at once.
- The order is the order of the sends: each subscription sees every value it does not lose in that order.
- `capacity` is rounded up to a power of two, at least 1. Up to 4095 subscriptions at once
  ([subscribe](subscribe.md) throws `length_error` beyond), and up to 2^52 values over the life of a
  broadcast: at a hundred million sends a second, a bus runs for over a year.
- A send between the reservation of its position and its store holds up the readers of that position and the senders
  a lap behind it for a few instructions, a spin: a slot is taken for the next lap only once the position it holds is
  committed, so the senders run at most a ring ahead of the commit point. Nothing else in the broadcast waits for
  anything but a value.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the values: a copy-constructible object type, copied to every subscription (a `static_assert` says so otherwise). A `tracked_ptr` is the usual `T`. |

## Member types

| Type | Definition |
|---|---|
| `value_type` | `T` |
| `size_type` | `size_t` |
| [subscription](../broadcast-subscription/README.md) | a receiver with a cursor of its own into the ring |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](broadcast.md) | makes a broadcast with a ring of a capacity |
| `(destructor)` | lets go of the ring, which the subscriptions alive keep |

#### Operations

| Function | Description |
|---|---|
| [subscribe](subscribe.md) | a subscription starting at the next value sent |
| [send](send.md) | sends a value to every subscription, without waiting |
| [close](close.md) | ends every subscription |

#### Observers

| Function | Description |
|---|---|
| [closed](closed.md) | checks whether the broadcast is closed |
| [capacity](capacity.md) | the number of values the ring holds |
| [subscribers](subscribers.md) | the number of subscriptions alive |

## Complexity

- A send: the allocation of the value's node, an atomic add, a store, the commit, and a walk over the subscriptions,
  a load each; linear in the number of subscriptions.
- A receive that finds its value: constant, a copy of the value. A subscriber that finds several values waiting takes
  them without a wake between, so a bus that sends in bursts pays the wake once per burst per subscriber.
- A subscription dropped counts itself off the values still in the ring, at most the capacity, whatever its lag.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Message {
    int number;
};

using Bus = async::broadcast<tracked_ptr<Message>>;

async::task<int> count_all(Bus::subscription messages) {
    int count = 0;
    while (co_await messages.receive()) {  // no thread held between messages
        ++count;
    }
    co_return count;
}

async::task<int> sum_all(Bus::subscription messages) {
    int sum = 0;
    while (auto m = co_await messages.receive()) {
        sum += (*m)->number;
    }
    co_return sum;
}

int main() {
    Bus bus(16);
    async::task<int> counter = async::spawn(count_all(bus.subscribe()));
    async::task<int> summer = async::spawn(sum_all(bus.subscribe()));
    for (int n : range(1, 11)) {
        bus.send(make_tracked<Message>(n));  // to both, without waiting
    }
    bus.close();  // what was sent is still received, then nothing
    println("{} messages", counter.wait());
    println("sum {}", summer.wait());
}
```

Output:

```text
10 messages
sum 55
```

## See also

- [subscription](../broadcast-subscription/README.md): the receiving side
- [channel](../channel/README.md): one receiver per element, and back-pressure
- [select](../select.md): a subscription's receive as a case
- [event](../event/README.md): one signal, to any number of waiters
- [task](../task/README.md), [scheduler](../scheduler/README.md): the tasks that receive and where they run

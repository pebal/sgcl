# Broadcast

```cpp
#include "sgcl/Sgcl/Async/Broadcast.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T>
    class Broadcast;                 // every subscriber receives every value
    // Broadcast<T>::Subscription    // a receiver with a cursor of its own
}
```

The same class in the `sgcl` interface: [broadcast](../../async/broadcast.md).

`Broadcast<T>` is a channel every subscriber receives every value from: tokio's `broadcast`, Kotlin's `SharedFlow`, the event bus of a user interface. A [Channel](Channel.md) hands each element to one receiver; here `b.Send(v)` goes to every `Subscription` alive, each reading at its own pace from one ring shared by all, the sender's position and a cursor per subscription. A subscription made later starts at the position of its making and sees what is sent from then on. The ring holds `capacity` values (rounded up to a power of two): a subscription that falls further behind loses the oldest, its next receive is the oldest still there, and `Lagged()` says how many were lost before it (tokio's `Lagged(n)`, as a count beside the value rather than an error in its place: `while (auto v = s.Receive()) { if (s.Lagged()) ...; Use(*v); }`). A value stays in the ring until every subscription alive at its send has passed it, or until it is lapped, and is let go of then: a `Ptr` value is held exactly that long, and a subscription dropped counts itself off the values it has not passed. `Close()` ends every subscription: what was sent is still received, then `None`. A subscription is received from the three ways of the module: a thread blocks on `s.Receive()`, a task `co_await s.AsyncReceive()`s (no thread held), a [Select](Select.md) takes `s.OnReceive(f)` as a case; a send never waits.

How it is made: one word holds the positions reserved so far and the number of subscriptions, together, so that a send takes the next position and the count of the subscriptions that will read it in one atomic add, and a subscription takes its cursor and counts itself in with another: the count on a value is exact, and the value's node lets the value go when the count reaches zero. The slots of the ring hold the values' nodes by atomic tracked pointers: a reader loads the node, copies the value out of it and counts itself off, and a sender overwriting the slot meanwhile frees nothing (the node is the collector's when nothing holds it), which is what lets readers read a slot no lock protects ([README: Lock-free containers](../../concurrent/README.md#lock-free-containers)). Senders publish in order: a second word counts the positions committed, advanced by whichever sender finds the next slot stored (a sender that stored ahead of a slower one leaves its commit to that one), so that a reader waits for a position by its commit alone. The wait is a channel of signals closed when a position is committed (a *round*: made by the first reader to wait for the position, closed and let go of by the sender that commits it), one round for all the readers waiting, since they all wait for the same position, the one committed next; the three ways of the wait are the round channel's three, and the value is taken after the wake. Measured once (Apple M-series, release): a send is 42 ns (the node's allocation and the words), whatever the number of subscriptions; each subscription that was waiting for it adds the scheduler's wake, which is the cost of any wake in the module (a task made ready: from a thread that is a worker woken through the kernel, a microsecond or so; from a task, the worker's own queue, less), so a send from a thread to one subscriber draining as fast as it can is ~150 ns and to eight ~6 µs, and from a task 42 ns and ~0.6–4 µs; a subscriber that finds several values waiting takes them without a wake between, so a bus that sends in bursts pays the wake once per burst per subscriber.

## Rules

- A `Broadcast` holds its ring by a `Ptr`, so it lives where one may: on a thread's stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1); it is neither copyable nor movable. A `Subscription` is movable, not copyable, and lives in the same places (a task's parameter or local, a member of a managed object); the ring outlives the `Broadcast` object while a subscription holds it.
- `T` is copied to every subscription (copy-constructible; a `Ptr` is the usual `T`).
- A send never waits and never fails but for the close: a value nobody subscribes to is dropped; a subscription that does not read is lapped, never a brake on the sender (Go's channel and tokio's `mpsc` apply back-pressure, tokio's `broadcast` does not).
- Each subscription is read by one thread or task at a time (its cursor is its own); many threads and tasks may send at once. A subscription dropped counts itself off the values still in the ring at most, whatever its lag. The positions are 52 bits: at a hundred million sends a second a bus runs for over a year.
- The order is the order of the sends: each subscription sees every value it does not lose in that order.
- `capacity` is rounded up to a power of two (at least 1). Up to 4095 subscriptions at once (`Subscribe()` throws `std::length_error` beyond); up to 2<sup>52</sup> values over the life of a broadcast.
- A send between the reservation of its position and its store holds up the readers of that position and the senders a lap behind it for a few instructions (a spin): a slot is taken for the next lap only once the position it holds is committed, so the senders run at most a ring ahead of the commit point; nothing else in the broadcast waits for anything but a value.

## Members

### Broadcast

```cpp
explicit Broadcast(SizeType capacity);         // a ring of `capacity` values, rounded up to a power of two
Subscription Subscribe();                      // a receiver from the next value on
bool Send(const T& value);  bool Send(T&& value);   // to every subscription alive, without waiting; false once closed
void Close();                                  // no more sends: every subscription receives what was sent, then None
bool IsClosed() const noexcept;
SizeType Capacity() const noexcept;
SizeType SubscriberCount() const noexcept;    // the subscriptions alive
```

### Subscription

```cpp
Optional<T> Receive();                         // the next value, waiting for one; None once closed and drained
Optional<T> TryReceive();                      // the next value if one is there
auto AsyncReceive() noexcept;                  // co_await: the next value, no thread held
template<class F> auto OnReceive(F f);         // a case of a Select: f(T), or f(Optional<T>) also called with None when closed and drained
SizeType Lagged() const noexcept;              // the values lost before the last one received
bool IsClosed() const noexcept;
explicit operator bool() const noexcept;       // not a moved-from or default one
```

```cpp
Broadcast<int> b(64);
Broadcast<int>::Subscription s = b.Subscribe();
b.Send(1);                                      // to s, without waiting
Optional<int> v = s.Receive();                  // 1
auto worker = [](Broadcast<int>::Subscription s) -> Task<> {
    while (auto v = co_await s.AsyncReceive()) {   // every value until the close
        if (s.Lagged()) { /* the ring lapped this subscriber: s.Lagged() values lost before *v */ }
    }
};
Channel<void> quit;
auto loop = [](Broadcast<int>::Subscription s, Channel<void>& quit) -> Task<> {
    for (bool on = true; on;) {
        co_await AsyncSelect(
            s.OnReceive([](int v) { /* a value */ }),
            quit.OnReceive([&] { on = false; }));
    }
};
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

using namespace std::chrono_literals;

// A message bus: every listener gets every message; a listener that joins
// late gets the messages from then on; one that reads too slowly loses the
// oldest and is told how many. The messages are managed objects, held by
// the ring until every listener has passed them.
struct Message {
    explicit Message(int n)
    : number(n) {
    }

    int number;
};

using Bus = Broadcast<Ptr<Message>>;

Task<int> CountAll(Bus::Subscription messages) {
    int count = 0;
    while (auto e = co_await messages.AsyncReceive()) {   // no thread held between messages
        ++count;
    }
    co_return count;
}

Task<int> SumAll(Bus::Subscription messages) {
    int sum = 0;
    while (auto e = co_await messages.AsyncReceive()) {
        sum += (*e)->number;
    }
    co_return sum;
}

int main() {
    Bus bus(4);                                          // the ring keeps the last four messages
    Task<int> counter = Spawn(CountAll(bus.Subscribe()));
    Task<int> summer = Spawn(SumAll(bus.Subscribe()));
    Bus::Subscription slow = bus.Subscribe();            // subscribed now, read at the end
    for (int n : Range(1, 6)) {
        bus.Send(Make<Message>(n));                      // to the three, without waiting
        ThisThread::SleepFor(1ms);                       // the listeners keep up; the slow one does not read
    }
    Task<int> late = Spawn(SumAll(bus.Subscribe()));     // from message 6 on
    for (int n : Range(6, 11)) {
        bus.Send(Make<Message>(n));
        ThisThread::SleepFor(1ms);
    }
    bus.Close();                                         // what was sent is still received, then None
    std::cout << "counter: " << counter.Join() << " messages\n";
    std::cout << "summer: " << summer.Join() << "\n";
    std::cout << "late: " << late.Join() << " (6 + 7 + 8 + 9 + 10)\n";
    Optional<Ptr<Message>> first = slow.Receive();
    std::cout << "slow: lost " << slow.Lagged() << ", then message " << (*first)->number;
    while (auto e = slow.Receive()) {
        std::cout << ", " << (*e)->number;
    }
    std::cout << "\n";
    Scheduler::Stop();
}
```

The output:

```
counter: 10 messages
summer: 55
late: 40 (6 + 7 + 8 + 9 + 10)
slow: lost 6, then message 7, 8, 9, 10
```

## See also

- [Channel](Channel.md): one receiver per element, and back-pressure; [Select](Select.md): the cases; [Mutex](Mutex.md) and its family: the synchronization of tasks; [Task](Task.md), [Scheduler](Scheduler.md): the tasks and their workers
- `tests/Sgcl/sync_and_broadcast.cpp`: the behaviour above, checked.

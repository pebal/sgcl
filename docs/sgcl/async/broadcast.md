# sgcl::broadcast

```cpp
#include "sgcl/async/broadcast.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class broadcast;                 // every subscriber receives every value
    // broadcast<T>::subscription    // a receiver with a cursor of its own
}
```

The same class in the `Sgcl` interface: [Broadcast](../Sgcl/Async/Broadcast.md).

`sgcl::broadcast<T>` is a channel every subscriber receives every value from: tokio's `broadcast`, Kotlin's `SharedFlow`, the event bus of a user interface. A [channel](channel.md) hands each element to one receiver; here `b.send(v)` goes to every `subscription` alive, each reading at its own pace from one ring shared by all, the sender's position and a cursor per subscription. A subscription made later starts at the position of its making and sees what is sent from then on. The ring holds `capacity` values (rounded up to a power of two): a subscription that falls further behind loses the oldest, its next receive is the oldest still there, and `lagged()` says how many were lost before it (tokio's `Lagged(n)`, as a count beside the value rather than an error in its place: `while (auto v = s.receive()) { if (s.lagged()) ...; use(*v); }`). A value stays in the ring until every subscription alive at its send has passed it, or until it is lapped, and is let go of then: a `tracked_ptr` value is held exactly that long, and a subscription dropped counts itself off the values it has not passed. `close()` ends every subscription: what was sent is still received, then nothing. A subscription is received from the three ways of the module: a thread blocks on `s.receive()`, a task `co_await s.async_receive()`s (no thread held), a [select](select.md) takes `s.on_receive(f)` as a case; a send never waits.

How it is made: one word holds the positions reserved so far and the number of subscriptions, together, so that a send takes the next position and the count of the subscriptions that will read it in one atomic add, and a subscription takes its cursor and counts itself in with another: the count on a value is exact, and the value's node lets the value go when the count reaches zero. The slots of the ring hold the values' nodes by atomic tracked pointers: a reader loads the node, copies the value out of it and counts itself off, and a sender overwriting the slot meanwhile frees nothing (the node is the collector's when nothing holds it), which is what lets readers read a slot no lock protects ([README: Lock-free containers](../concurrent/README.md#lock-free-containers)). Senders publish in order: a second word counts the positions committed, advanced by whichever sender finds the next slot stored (a sender that stored ahead of a slower one leaves its commit to that one), so that a reader waits for a position by its commit alone. The wait of a receive is kept with the subscription (tokio's shape, the waker in the receiver): the position it waits for, its task's frame or its thread's park word, and a link in the list of subscriptions the senders walk. A receive that finds nothing registers the position and looks once more; the sender that commits past it walks the subscriptions, claims each registration behind the commit with a compare-exchange and wakes that one subscriber once, a task handed to the scheduler, a thread through its word; nothing is allocated and no list shared by every waiter is pushed on per value (it was a *round* before: a channel made by the first reader waiting for a position and closed by the sender that committed it, every reader registering on it anew for every value, and sixteen subscribers cost 40 µs per value between threads and 8.5 between tasks). A thread looks for the next commit for a few microseconds before it parks, as the queues do, since a sender at work is nanoseconds from it. A select case keeps a round of its own, since a case must be served with a value and a signal kept with a subscription could be a stale one. Measured with `benchmarks/compare.sh` (`bcast`, one sender thread of a million values, every subscriber receiving them all): 135 ns per value with one thread subscriber, 620 with four, 1.5 µs with sixteen, and 193 µs with sixty-four, more threads than the machine's cores, where the ones without a core park in the kernel and are woken one by one; with task subscribers 0.14, 1.0, 6.5 and 25 µs, a task's wake being a push on the scheduler's queue and a worker woken for it (Go's idiom, a channel per subscriber and a goroutine on each: 46 ns, 199, 1249 and 13.5 µs; the table with the three columns is on [the concurrent benchmarks page](../concurrent/benchmarks.md#the-single-producer-queue-the-cache-and-the-persistent-map)). A send costs the node's allocation, the words and the walk, a load per subscription; a subscriber that finds several values waiting takes them without a wake between, so a bus that sends in bursts pays the wake once per burst per subscriber.

## Rules

- A `broadcast` holds its ring by a `tracked_ptr`, so it lives where one may: on a thread's stack or inside a managed object ([The rules](../core/README.md#the-rules), 1); it is neither copyable nor movable. A `subscription` is movable, not copyable, and lives in the same places (a task's parameter or local, a member of a managed object); the ring outlives the `broadcast` object while a subscription holds it.
- `T` is copied to every subscription (copy-constructible; a `tracked_ptr` is the usual `T`).
- A send never waits and never fails but for the close: a value nobody subscribes to is dropped; a subscription that does not read is lapped, never a brake on the sender (Go's channel and tokio's `mpsc` apply back-pressure, tokio's `broadcast` does not).
- Each subscription is read by one thread or task at a time (its cursor is its own); many threads and tasks may send at once. A subscription dropped counts itself off the values still in the ring at most, whatever its lag. The positions are 52 bits: at a hundred million sends a second a bus runs for over a year.
- The order is the order of the sends: each subscription sees every value it does not lose in that order.
- `capacity` is rounded up to a power of two (at least 1). Up to 4095 subscriptions at once (`subscribe()` throws `std::length_error` beyond); up to 2<sup>52</sup> values over the life of a broadcast.
- A send between the reservation of its position and its store holds up the readers of that position and the senders a lap behind it for a few instructions (a spin): a slot is taken for the next lap only once the position it holds is committed, so the senders run at most a ring ahead of the commit point; nothing else in the broadcast waits for anything but a value.

## Members

### broadcast

```cpp
explicit broadcast(size_type capacity);        // a ring of `capacity` values, rounded up to a power of two
subscription subscribe();                      // a receiver from the next value on
bool send(const T& value);  bool send(T&& value);   // to every subscription alive, without waiting; false once closed
void close();                                  // no more sends: every subscription receives what was sent, then nothing
bool closed() const noexcept;
size_type capacity() const noexcept;
size_type subscribers() const noexcept;       // the subscriptions alive
```

### subscription

```cpp
optional<T> receive();                         // the next value, waiting for one; nothing once closed and drained
optional<T> try_receive();                     // the next value if one is there
auto async_receive() noexcept;                 // co_await: the next value, no thread held
template<class F> auto on_receive(F f);        // a case of a select: f(T), or f(optional<T>) also called with nothing when closed and drained
size_t lagged() const noexcept;                // the values lost before the last one received
bool closed() const noexcept;
explicit operator bool() const noexcept;       // not a moved-from or default one
```

```cpp
sgcl::broadcast<int> b(64);
sgcl::broadcast<int>::subscription s = b.subscribe();
b.send(1);                                      // to s, without waiting
sgcl::optional<int> v = s.receive();            // 1
auto worker = [](sgcl::broadcast<int>::subscription s) -> sgcl::task<> {
    while (auto v = co_await s.async_receive()) {   // every value until the close
        if (s.lagged()) { /* the ring lapped this subscriber: s.lagged() values lost before *v */ }
    }
};
sgcl::channel<void> quit;
auto loop = [](sgcl::broadcast<int>::subscription s, sgcl::channel<void>& quit) -> sgcl::task<> {
    for (bool on = true; on;) {
        co_await sgcl::async_select(
            s.on_receive([](int v) { /* a value */ }),
            quit.on_receive([&] { on = false; }));
    }
};
```

## Example

```cpp
#include "sgcl/sgcl.h"
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

using Bus = sgcl::broadcast<sgcl::tracked_ptr<Message>>;

sgcl::task<int> count_all(Bus::subscription messages) {
    int count = 0;
    while (auto e = co_await messages.async_receive()) {   // no thread held between messages
        ++count;
    }
    co_return count;
}

sgcl::task<int> sum_all(Bus::subscription messages) {
    int sum = 0;
    while (auto e = co_await messages.async_receive()) {
        sum += (*e)->number;
    }
    co_return sum;
}

int main() {
    Bus bus(4);                                          // the ring keeps the last four messages
    sgcl::task<int> counter = sgcl::spawn(count_all(bus.subscribe()));
    sgcl::task<int> summer = sgcl::spawn(sum_all(bus.subscribe()));
    Bus::subscription slow = bus.subscribe();            // subscribed now, read at the end
    for (int n : sgcl::range(1, 6)) {
        bus.send(sgcl::make_tracked<Message>(n));        // to the three, without waiting
        std::this_thread::sleep_for(1ms);                // the listeners keep up; the slow one does not read
    }
    sgcl::task<int> late = sgcl::spawn(sum_all(bus.subscribe()));   // from message 6 on
    for (int n : sgcl::range(6, 11)) {
        bus.send(sgcl::make_tracked<Message>(n));
        std::this_thread::sleep_for(1ms);
    }
    bus.close();                                         // what was sent is still received, then nothing
    std::cout << "counter: " << counter.join() << " messages\n";
    std::cout << "summer: " << summer.join() << "\n";
    std::cout << "late: " << late.join() << " (6 + 7 + 8 + 9 + 10)\n";
    sgcl::optional<sgcl::tracked_ptr<Message>> first = slow.receive();
    std::cout << "slow: lost " << slow.lagged() << ", then message " << (*first)->number;
    while (auto e = slow.receive()) {
        std::cout << ", " << (*e)->number;
    }
    std::cout << "\n";
    sgcl::scheduler::stop();
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

- [channel](channel.md): one receiver per element, and back-pressure; [select](select.md): the cases; [mutex](mutex.md) and its family: the synchronization of tasks; [coroutine](coroutine.md), [scheduler](scheduler.md): the tasks and their workers
- `tests/async/broadcast.cpp`: every behaviour above, checked.

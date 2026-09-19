# Sgcl::Channel

```cpp
#include "sgcl/Sgcl/Async/Channel.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T>
    class Channel;
    template<>
    class Channel<void>;   // a channel of signals
}
```

The same class in the `sgcl` interface: [channel](../../async/channel.md).

`Channel<T>` is the channel of Go: a queue with the synchronization of both ends. A channel of capacity *n* buffers *n* elements; one of capacity 0 buffers none, and a send waits until a receive takes the element, so that the pair is a meeting of the two sides (a rendezvous) and not a delivery to a buffer. A receive on an empty channel waits, a send on a full one waits: a producer ahead of its consumer stops (back-pressure). `Close()` ends the stream: what was sent is still received, then every receive returns nothing at once and every send returns `false`; a range-for over the channel runs until then.

The waiting is done by a thread, on an atomic of its own, or by a coroutine: `co_await ch.AsyncReceive()` and `co_await ch.AsyncSend(v)` suspend the coroutine with its handle on the channel's list of waiters, and the send or the receive that serves it hands it to the [scheduler](Scheduler.md), which runs it on a worker; the serving thread returns at once. The buffer is a lock-free ring (the bounded queue of Vyukov: a managed array of slots made once, a sequence number per slot, one compare-exchange per send or receive and no allocation per element), the lists of waiters are [ConcurrentQueue](../Concurrent/ConcurrentQueue.md)s, and every waiter is a managed object: nothing in the channel takes a lock, nothing frees anything, and a waiter that was cancelled or served is reclaimed by the collector ([README: Lock-free containers](../../concurrent/README.md#lock-free-containers)). A rendezvous has a small ring too, through which a waiting sender's element passes to the receiver that serves it, so that waiting senders are served in their order.

## Rules

- The channel holds its queues by `Ptr`s, so it lives where one may: on a thread's stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1).
- Every operation is lock-free on the channel's side; `Send` and `Receive` wait only for the other side, `TrySend` and `TryReceive` never. Elements are delivered in the order each sender sent them; between senders sending at the same moment the order is theirs.
- A coroutine that awaits a channel must have a managed frame (a [Task](Coroutine.md); a `static_assert` says so otherwise), so that the tracked pointers of its frame are roots while it waits; the channel's waiter holds the frame for the length of the wait, so a task nobody holds may wait. The thread that serves the wait makes the coroutine ready on the scheduler and goes on; a worker runs the coroutine. A coroutine's waiter is on the list before the coroutine looks at the channel one last time (a send or a receive that came meanwhile), and is published to the serving side only after that look: nothing serves it before, so the coroutine cannot be resumed, finished and its channel destroyed while the look still reads it; a thread's wait has no such window, since the thread blocks and its channel stays.
- A send to a closed channel returns `false` (Go panics); a send waiting when the channel closes returns `false` with its element undelivered. `Close()` twice is nothing.
- `Count()` is the elements in the buffer; `IsEmpty()` is no element in the buffer and no sender waiting with one.
- Non-copyable, non-movable.

## Members

### Types

```cpp
using ValueType = T;
using InnerType = sgcl::channel<T>;
using SizeType = size_t;
```

### Constructors

```cpp
explicit Channel(SizeType capacity = 0);   // 0: a rendezvous
Channel(const Channel&) = delete;
```

### Send, TrySend, AsyncSend

```cpp
bool Send(const T& value);
bool Send(T&& value);
bool TrySend(const T& value);
bool TrySend(T&& value);
auto AsyncSend(const T& value);   // co_await gives the bool of Send
auto AsyncSend(T&& value);
```

`Send` delivers the element: to a waiting receiver, into the buffer when it has room, or after waiting for a receiver to make room (or, on a rendezvous, to take it); `true`, or `false` when the channel is closed. `TrySend` delivers without waiting: `false` when closed, full, or, on a rendezvous, when no receiver waits.

### Receive, TryReceive, AsyncReceive

```cpp
Optional<T> Receive();
Optional<T> TryReceive();
auto AsyncReceive();   // co_await gives the Optional<T> of Receive
```

`Receive` takes the next element, waiting for one; `None` once the channel is closed and drained. `TryReceive` takes without waiting; `None` when there is nothing.

### Close, IsClosed, Capacity, Count, IsEmpty

```cpp
void Close();
bool IsClosed() const noexcept;
SizeType Capacity() const noexcept;
SizeType Count() const noexcept;
bool IsEmpty() const noexcept;
```

### begin, end

```cpp
auto begin(Channel&);              // free functions: an input iterator that receives on each step
auto end(Channel&) noexcept;       // the end once nothing comes
```

`for (auto v : ch)` receives until the channel is closed and drained.

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

### Channel<void>

```cpp
bool Send();
bool TrySend();
bool Receive();        // whether a signal came, false once closed
bool TryReceive();
auto AsyncSend();
auto AsyncReceive();   // co_await gives the bool
void Close(); bool IsClosed() const noexcept; SizeType Capacity() const noexcept; SizeType Count() const noexcept; bool IsEmpty() const noexcept;
```

A channel that carries nothing but the fact of a send: a signal of readiness, a cancellation.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// A pipeline of coroutines and threads: producers send jobs on one
// channel, a coroutine turns each into a result on another, a thread
// collects; nobody locks, nobody frees, the buffers hold the producers
// back when the worker is behind
struct Job {
    int id;
};

Task<> Worker(Channel<Ptr<Job>>& jobs, Channel<int>& results) {
    while (auto job = co_await jobs.AsyncReceive()) {      // suspends while jobs is empty
        co_await results.AsyncSend((*job)->id * 2);        // suspends while results is full
    }
    results.Close();                                       // jobs closed: the stream ends downstream
}

int main() {
    Channel<Ptr<Job>> jobs(8);
    Channel<int> results(8);
    Task w = Spawn(Worker(jobs, results));   // on the scheduler: runs whenever a job comes
    List<Thread> producers;
    for (int p : Range(4)) {
        producers.Emplace([&, p] {
            for (int i : Range(100)) {
                jobs.Send(Make<Job>(p * 100 + i));   // waits when the buffer of eight is full
            }
        });
    }
    long sum = 0;
    Thread collector([&] {
        for (int r : results) {                            // until results is closed
            sum += r;
        }
    });
    for (auto& t : producers) {
        t.Join();
    }
    jobs.Close();
    w.Join();                                              // the worker's loop ended with the close
    collector.Join();
    std::cout << sum << "\n";
    return sum == 2 * 399 * 400 / 2 ? 0 : 1;
}
```

The output:

```
159600
```

## See also

- [Select](Select.md): a wait on several channels at once; [ConcurrentQueue](../Concurrent/ConcurrentQueue.md), the lists of waiters; [Task](Coroutine.md), the tasks that await a channel; [Scheduler](Scheduler.md), what runs them
- [README: Lock-free containers](../../concurrent/README.md#lock-free-containers)

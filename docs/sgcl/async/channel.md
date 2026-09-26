# sgcl::async::channel

```cpp
#include "sgcl/async/channel.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class channel;
    template<>
    class channel<void>;   // a channel of signals
}
```

`sgcl::async::channel<T>` is the channel of Go: a queue with the synchronization of both ends. A channel of capacity *n* buffers *n* elements; one of capacity 0 buffers none, and a send waits until a receive takes the element, so that the pair is a meeting of the two sides (a rendezvous) and not a delivery to a buffer. A receive on an empty channel waits, a send on a full one waits: a producer ahead of its consumer stops (back-pressure). `close()` ends the stream: what was sent is still received, then every receive returns nothing at once and every send returns `false`; a range-for over the channel runs until then.

The waiting is done by a thread, on an atomic of its own, or by a coroutine: `co_await ch.receive()` and `co_await ch.send(v)` suspend the coroutine with its handle on the channel's list of waiters, and the send or the receive that serves it hands it to the [scheduler](scheduler.md), which runs it on a worker; the serving thread returns at once. The buffer is a lock-free ring (the bounded queue of Vyukov: a managed array of slots made once, a sequence number per slot, one compare-exchange per send or receive and no allocation per element), the lists of waiters are [concurrent::queue](../concurrent/queue.md)s, and every waiter is a managed object: nothing in the channel takes a lock, nothing frees anything, and a waiter that was cancelled or served is reclaimed by the collector ([README: Lock-free containers](../concurrent/README.md#lock-free-containers). A rendezvous has a small ring too, through which a waiting sender's element passes to the receiver that serves it, so that waiting senders are served in their order. A buffered channel keeps a count of the entries of each list beside it, so that a send that has pushed and a receive that has popped, with nobody waiting, which is the common case, look at one word rather than at the head of a queue (the [benchmarks](../concurrent/benchmarks.md): 30 ns for a send and a receive on one thread against 60 without the count); a rendezvous, which serves every element through the lists, keeps none.

## Rules

- The channel holds its queues by `tracked_ptr`s, so it lives where one may: on a thread's stack or inside a managed object ([The rules](../core/README.md#the-rules), 1).
- Every operation is lock-free on the channel's side; `send` and `receive` wait only for the other side, `try_send` and `try_receive` never. Elements are delivered in the order each sender sent them; between senders sending at the same moment the order is theirs.
- A coroutine that awaits a channel must have a managed frame (a [task](coroutine.md), or a promise derived from `managed_frame`; a `static_assert` says so otherwise), so that the tracked pointers of its frame are roots while it waits; the channel's waiter holds the frame for the length of the wait, so a detached task may wait. A coroutine's waiter is on the list before the coroutine looks at the channel one last time (a send or a receive that came meanwhile), and is published to the serving side only after that look: nothing serves it before, so the coroutine cannot be resumed, finished and its channel destroyed while the look still reads it; a thread's wait has no such window, since the thread blocks and its channel stays. The thread that serves the wait makes the coroutine ready on the scheduler and goes on; a worker runs the coroutine.
- A send to a closed channel returns `false` (Go panics); a send waiting when the channel closes returns `false` with its element undelivered. `close()` twice is nothing.
- `size()` is the elements in the buffer; `empty()` is no element in the buffer and no sender waiting with one.
- Non-copyable, non-movable.

## Members

### Types

```cpp
using value_type = T;
using size_type = size_t;
class iterator;   // an input iterator that receives on each step, the end once nothing comes
```

### Constructors

```cpp
explicit channel(size_type capacity = 0);   // 0: a rendezvous
channel(const channel&) = delete;
```

### send, try_send, send

```cpp
auto send(const T& value);   // an operation: co_await ch.send(v) in a task, ch.send(v).wait() on a thread; true when delivered
auto send(T&& value);
bool try_send(const T& value);
bool try_send(T&& value);
```

`send` delivers the element: to a waiting receiver, into the buffer when it has room, or after waiting for a receiver to make room (or, on a rendezvous, to take it); `true`, or `false` when the channel is closed. `try_send` delivers without waiting: `false` when closed, full, or, on a rendezvous, when no receiver waits.

### receive, try_receive, receive

```cpp
auto receive();   // an operation: co_await ch.receive() in a task, ch.receive().wait() on a thread; optional<T>, nothing once closed and drained
optional<T> try_receive();
```

`receive` takes the next element, waiting for one; nothing once the channel is closed and drained. `try_receive` takes without waiting; nothing when there is nothing.

### close, closed, capacity, size, empty

```cpp
void close();
bool closed() const noexcept;
size_type capacity() const noexcept;
size_type size() const noexcept;
bool empty() const noexcept;
```

### begin, end

```cpp
iterator begin();
iterator end() noexcept;
```

`for (auto v : ch)` receives until the channel is closed and drained.

### channel<void>

```cpp
auto send();     // operations, as above: co_await or  gives a bool
auto receive();  // whether a signal came, false once closed
bool try_send();
bool try_receive();
void close(); bool closed() const noexcept; size_type capacity() const noexcept; size_type size() const noexcept; bool empty() const noexcept;
```

A channel that carries nothing but the fact of a send: a signal of readiness, a cancellation.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// A pipeline of coroutines and threads: producers send jobs on one
// channel, a coroutine turns each into a result on another, a thread
// collects; nobody locks, nobody frees, the buffers hold the producers
// back when the worker is behind
struct Job {
    int id;
};

async::task<> worker(async::channel<tracked_ptr<Job>>& jobs, async::channel<int>& results) {
    while (auto job = co_await jobs.receive()) {     // suspends while jobs is empty
        co_await results.send((*job)->id * 2);       // suspends while results is full
    }
    results.close();                                       // jobs closed: the stream ends downstream
}

int main() {
    async::channel<tracked_ptr<Job>> jobs(8);
    async::channel<int> results(8);
    async::task w = async::spawn(worker(jobs, results));   // on the scheduler: runs whenever a job comes
    vector<thread> producers;
    for (int p : range(4)) {
        producers.emplace_back([&, p] {
            for (int i : range(100)) {
                jobs.send(make_tracked<Job>(p * 100 + i)).wait();   // waits when the buffer of eight is full
            }
        });
    }
    long sum = 0;
    thread collector([&] {
        for (int r : results) {                            // until results is closed
            sum += r;
        }
    });
    for (auto& t : producers) {
        t.join();
    }
    jobs.close();
    w.wait();                                              // the worker's loop ended with the close
    collector.join();
    std::cout << sum << "\n";
    return sum == 2 * 399 * 400 / 2 ? 0 : 1;
}
```

The output:

```
159600
```

## See also

- [select](select.md): a wait on several channels at once; [concurrent::queue](../concurrent/queue.md), the lists of waiters; [coroutine](coroutine.md), the tasks that await a channel; [scheduler](scheduler.md), what runs them
- [README: Lock-free containers](../concurrent/README.md#lock-free-containers)

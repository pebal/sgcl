# sgcl::channel

```cpp
#include "sgcl/channel.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T, template<class> class Ptr = tracked_ptr>
    class channel;
    template<template<class> class Ptr>
    class channel<void, Ptr>;   // a channel of signals
}
namespace gc {
    template<class T>
    using channel = sgcl::channel<T, gc::tracked_ptr>;
}
```

`sgcl::channel<T>` is the channel of Go: a queue with the synchronization of both ends. A channel of capacity *n* buffers *n* elements; one of capacity 0 buffers none, and a send waits until a receive takes the element, so that the pair is a meeting of the two sides (a rendezvous) and not a delivery to a buffer. A receive on an empty channel waits, a send on a full one waits: a producer ahead of its consumer stops (back-pressure). `close()` ends the stream: what was sent is still received, then every receive returns nothing at once and every send returns `false`; a range-for over the channel runs until then.

The waiting is done by a thread, on an atomic of its own, or by a coroutine: `co_await ch.async_receive()` and `co_await ch.async_send(v)` suspend the coroutine with its handle on the channel's list of waiters, and the send or the receive that serves it resumes it, on the serving thread. The buffer is a [concurrent_queue](concurrent_queue.md), the lists of waiters are concurrent queues too, and every waiter is a managed object: nothing in the channel takes a lock, nothing frees anything, and a waiter that was cancelled or served is reclaimed by the collector, as the elements are ([README: Lock-free containers](../README.md#lock-free-containers)).

## Rules

- The channel holds its queues by words of the `Ptr` kind: `sgcl::channel` lives where a `tracked_ptr` may, on a thread's stack or inside a managed object ([The rules](../README.md#the-rules), 1); `gc::channel` lives anywhere.
- Every operation is lock-free on the channel's side; `send` and `receive` wait only for the other side, `try_send` and `try_receive` never. Elements are delivered in the order each sender sent them; between senders sending at the same moment the order is theirs.
- A coroutine that awaits a channel must have a managed frame (a [task](coroutine.md), or a promise derived from `managed_frame`), so that the tracked pointers of its frame are roots while it waits; its frame is held by its task for the length of the wait, as for any suspension. The thread that serves the wait resumes the coroutine and runs it until its next suspension: a send from a thread runs the receiving coroutine on that thread.
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

### send, try_send, async_send

```cpp
bool send(const T& value);
bool send(T&& value);
bool try_send(const T& value);
bool try_send(T&& value);
auto async_send(const T& value);   // co_await gives the bool of send
auto async_send(T&& value);
```

`send` delivers the element: to a waiting receiver, into the buffer when it has room, or after waiting for a receiver to make room (or, on a rendezvous, to take it); `true`, or `false` when the channel is closed. `try_send` delivers without waiting: `false` when closed, full, or, on a rendezvous, when no receiver waits.

### receive, try_receive, async_receive

```cpp
optional<T> receive();
optional<T> try_receive();
auto async_receive();   // co_await gives the optional<T> of receive
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
bool send();
bool try_send();
bool receive();        // whether a signal came, false once closed
bool try_receive();
auto async_send();
auto async_receive();  // co_await gives the bool
void close(); bool closed() const noexcept; size_type capacity() const noexcept; size_type size() const noexcept; bool empty() const noexcept;
```

A channel that carries nothing but the fact of a send: a signal of readiness, a cancellation.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>
#include <thread>
#include <vector>

// A pipeline of coroutines and threads: producers send jobs on one
// channel, a coroutine turns each into a result on another, a thread
// collects; nobody locks, nobody frees, the buffers hold the producers
// back when the worker is behind
struct Job {
    int id;
};

gc::task<> worker(gc::channel<gc::tracked_ptr<Job>>& jobs, gc::channel<int>& results) {
    while (auto job = co_await jobs.async_receive()) {     // suspends while jobs is empty
        co_await results.async_send((*job)->id * 2);       // suspends while results is full
    }
    results.close();                                       // jobs closed: the stream ends downstream
}

int main() {
    gc::channel<gc::tracked_ptr<Job>> jobs(8);
    gc::channel<int> results(8);
    gc::task<> w = worker(jobs, results);
    w.resume();                                            // to its first wait; from then on the senders drive it
    std::vector<std::thread> producers;
    for (int p = 0; p < 4; ++p) {
        producers.emplace_back([&, p] {
            for (int i = 0; i < 100; ++i) {
                jobs.send(gc::make_tracked<Job>(p * 100 + i));   // waits when the buffer of eight is full
            }
        });
    }
    long sum = 0;
    std::thread collector([&] {
        for (int r : results) {                            // until results is closed
            sum += r;
        }
    });
    for (auto& t : producers) {
        t.join();
    }
    jobs.close();
    collector.join();
    std::cout << sum << "\n";
    return sum == 2 * 399 * 400 / 2 ? 0 : 1;
}
```

## See also

- [concurrent_queue](concurrent_queue.md), the buffer and the lists of waiters; [coroutine](coroutine.md), the tasks that await a channel
- [README: Lock-free containers](../README.md#lock-free-containers)

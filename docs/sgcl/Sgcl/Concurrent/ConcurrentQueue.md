# Sgcl::ConcurrentQueue

```cpp
#include "sgcl/Sgcl/Concurrent/ConcurrentQueue.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T>
    class ConcurrentQueue;
}
```

The same class in the `sgcl` interface: [concurrent_queue](../../concurrent/concurrent_queue.md).

`ConcurrentQueue<T>` is an unbounded lock-free FIFO queue shared by any number of producers and consumers: the Michael–Scott queue in the form Java's `ConcurrentLinkedQueue` gives it, written as it is written for a runtime with a collector. The nodes form a singly linked list; the head addresses a node at or before the first element and the tail a node at or before the last one, both lagging on purpose. An enqueue links the new node after the last one with a compare-exchange on that node's link and swings the tail only when it found the tail a node or more behind; a dequeue walks from the head to the first element not yet taken, claims it with a compare-exchange on its node's flag, and swings the head only when the element was a node or more past it. That halves the exchanges on the two words every thread contends for (Java's "hop two nodes at a time"), and a thread that finds a word behind walks the links to where it should be, so no thread ever waits for another. A node the head has passed is linked to itself: the sign, for a walk, that it left the list, and the reason an old head a thread still holds retains nothing behind it. No ABA, no counted pointers, no hazard pointers in the algorithm and no free list: a node is never reused while a thread holds it, and a node nobody holds is reclaimed by the collector ([README: Lock-free containers](../../concurrent/README.md#lock-free-containers)). The interface has the names of a queue: `Enqueue`, `Emplace`, `TryDequeue`, `Dequeue`, `IsEmpty`, `Count`, `Clear`. The element type is any movable `T`, a `Ptr` included.

## Rules

- The container is two atomic words, the head and the tail, kept a cache line apart (`config::CacheLineSize`) so that the consumers' line and the producers' line do not bounce for each other's traffic. The queue lives where a `Ptr` may: on a thread's stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1).
- Every operation is lock-free and may be called from any thread at any time; `Enqueue` and `TryDequeue` are linearizable at their compare-exchange on a link and on a node's flag. The queue is FIFO: every producer's elements come out in the order it enqueued them, at every consumer. `Dequeue` blocks while the queue is empty, on the link of the last node; every `Enqueue` notifies.
- An element is moved out of its node by the thread that dequeues it, into the `Optional` returned, and destroyed in the node there and then, on the dequeuing thread. The move should not throw: an element whose move constructor throws is lost.
- The nodes between the head and the first element, taken ones the head has not passed yet, are the queue's while it lives: at most a couple, as the head is swung every second node.
- `Count()` walks the nodes: linear, and a snapshot of no particular moment when other threads enqueue or dequeue, as Java's `size` is. `IsEmpty()` is a walk to the first element, a load or two.
- A `Ptr` may not address an element ([The rules](../../core/README.md#the-rules), 4); there is no `Peek()`: the first element is what `TryDequeue` returns.
- Non-copyable, non-movable: a shared structure has one place.

## Members

### Types

```cpp
using ValueType = T;
using InnerType = sgcl::concurrent_queue<T>;
using SizeType = size_t;
```

### Constructors

```cpp
ConcurrentQueue();
ConcurrentQueue(const ConcurrentQueue&) = delete;
```

An empty queue: one node on the managed heap whose element is taken, addressed by the head and the tail.

### Enqueue, Emplace

```cpp
void Enqueue(const T& value);
void Enqueue(T&& value);
template<class... A> void Emplace(A&&... a);
```

Creates a node on the managed heap holding the element (constructed from `a...` in place for `Emplace`), walks from the tail to the last node and links the new one after it with a compare-exchange on its link; swings the tail when the walk went two nodes or more (a failure there is another enqueue's success). Notifies the threads waiting in `Dequeue`.

```cpp
struct Request { int id; void Handle() {} };
ConcurrentQueue<Ptr<Request>> requests;
requests.Enqueue(Make<Request>(1));
requests.Emplace(Make<Request>(2));
```

### TryDequeue, Dequeue

```cpp
Optional<T> TryDequeue();
T Dequeue();
```

`TryDequeue` takes the first element: it walks from the head to the first node whose element is not taken and claims it with a compare-exchange on the node's flag; the element, or `None` when the walk reached the last node. `Dequeue` takes the first element, waiting while the queue is empty.

```cpp
struct Request { int id; void Handle() {} };
ConcurrentQueue<Ptr<Request>> requests;
requests.Enqueue(Make<Request>(1));
requests.Enqueue(Make<Request>(2));
while (auto r = requests.TryDequeue()) {
    (*r)->Handle();
}
requests.Enqueue(Make<Request>(3));
Ptr next = requests.Dequeue();   // blocks until an enqueue
```

### IsEmpty, Count

```cpp
bool IsEmpty() const noexcept;
SizeType Count() const noexcept;
```

`IsEmpty` walks from the head to the first element; `Count` counts the elements not taken, in linear time.

### Clear

```cpp
void Clear() noexcept;
```

Dequeues every element there is, destroying each on the calling thread.

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The queue inside, as its own type.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// A pipeline stage: producers enqueue messages, consumers dequeue them in
// order; the queue is a member of a managed object, and nothing in the
// program frees a node
struct Message {
    int producer, seq;
};

struct Stage {
    ConcurrentQueue<Ptr<Message>> inbox;   // inside a managed object
};

int main() {
    Ptr stage = Make<Stage>();
    Atomic<int> received = 0, outOfOrder = 0;
    List<Thread> threads;
    for (int p : Range(4)) {
        threads.Emplace([&, p] {
            for (int i : Range(1000)) {
                stage->inbox.Emplace(Make<Message>(p, i));
            }
        });
        threads.Emplace([&] {
            int last[4] = {-1, -1, -1, -1};
            for (int i : Range(1000)) {
                Ptr m = stage->inbox.Dequeue();   // FIFO per producer, at every consumer
                outOfOrder += m->seq <= last[m->producer];
                last[m->producer] = m->seq;
                ++received;
            }
        });
    }
    for (auto& t : threads) {
        t.Join();
    }
    std::cout << received << " messages, " << outOfOrder << " out of order\n";
    return received == 4000 && outOfOrder == 0 ? 0 : 1;
}
```

The output:

```
4000 messages, 0 out of order
```

## See also

- [ConcurrentStack](ConcurrentStack.md) for the LIFO counterpart, [ConcurrentSortedDictionary](ConcurrentSortedDictionary.md) for the ordered dictionary
- [Atomic](Atomic.md), what the head, the tail and the links are
- [Queue](../Containers/Queue.md), the sequential adapter; [Channel](../Async/Channel.md), a queue with the synchronization of both ends
- [README: Lock-free containers](../../concurrent/README.md#lock-free-containers), [README: The rules](../../core/README.md#the-rules)

# sgcl::concurrent_queue

```cpp
#include "sgcl/concurrent/concurrent_queue.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class concurrent_queue;
}
```

`sgcl::concurrent_queue<T>` is an unbounded lock-free FIFO queue shared by any number of producers and consumers: the Michael–Scott queue in the form Java's `ConcurrentLinkedQueue` gives it, written as it is written for a runtime with a collector. The nodes form a singly linked list; the head addresses a node at or before the first element and the tail a node at or before the last one, both lagging on purpose. A push links the new node after the last one with a compare-exchange on that node's link and swings the tail only when it found the tail a node or more behind; a pop walks from the head to the first element not yet taken, claims it with a compare-exchange on its node's flag, and swings the head only when the element was a node or more past it. That halves the exchanges on the two words every thread contends for (Java's "hop two nodes at a time"), and a thread that finds a word behind walks the links to where it should be, so no thread ever waits for another. A node the head has passed is linked to itself: the sign, for a walk, that it left the list, and the reason an old head a thread still holds retains nothing behind it. No ABA, no counted pointers, no hazard pointers in the algorithm and no free list: a node is never reused while a thread holds it, and a node nobody holds is reclaimed by the collector ([README: Lock-free containers](README.md#lock-free-containers)). The interface has the names of `std::queue`: `push`, `emplace`, `try_pop`, `pop`, `empty`, `size`, `clear`. The element type is any movable `T`, a `tracked_ptr` included.

## Rules

- The container is two atomic words, the head and the tail, kept a cache line apart (`config::CacheLineSize`) so that the consumers' line and the producers' line do not bounce for each other's traffic. The queue lives where a `tracked_ptr` may: on a thread's stack or inside a managed object ([The rules](../core/README.md#the-rules), 1).
- Every operation is lock-free and may be called from any thread at any time; `push` and `try_pop` are linearizable at their compare-exchange on a link and on a node's flag. The queue is FIFO: every producer's elements come out in the order it pushed them, at every consumer. `pop` blocks while the queue is empty, on the link of the last node; every `push` notifies.
- An element is moved out of its node by the thread that pops it, into the `optional` returned, and destroyed in the node there and then: what `std::queue::pop` does, on the popping thread. The move should not throw: an element whose move constructor throws is lost.
- The nodes between the head and the first element, taken ones the head has not passed yet, are the queue's while it lives: at most a couple, as the head is swung every second node.
- `size()` walks the nodes: linear, and a snapshot of no particular moment when other threads push or pop, as Java's `size` is. `empty()` is a walk to the first element, a load or two.
- A `tracked_ptr` may not address an element ([The rules](../core/README.md#the-rules), 4); there is no `front()`: the first element is what `try_pop` returns.
- Non-copyable, non-movable: a shared structure has one place.

## Members

### Types

```cpp
using value_type = T;
using size_type = size_t;
```

### Constructors

```cpp
concurrent_queue();
concurrent_queue(const concurrent_queue&) = delete;
```

An empty queue: one node on the managed heap whose element is taken, addressed by the head and the tail.

### push, emplace

```cpp
void push(const T& value);
void push(T&& value);
template<class... A> void emplace(A&&... a);
```

Creates a node on the managed heap holding the element (constructed from `a...` in place for `emplace`), walks from the tail to the last node and links the new one after it with a compare-exchange on its link; swings the tail when the walk went two nodes or more (a failure there is another push's success). Notifies the threads waiting in `pop`, when there are any: `pop` counts itself before its last look, and a push with nobody counted notifies nothing (a notify with nobody waiting is a fetch-add and a fence on a table the library shares, and a wake through the kernel now and then; the lists of waiters the async module keeps in these queues never block in `pop`, and the notify was half of a hop between two tasks over a rendezvous, measured).

```cpp
sgcl::concurrent_queue<sgcl::tracked_ptr<Request>> requests;   // a global: sgcl::
requests.push(sgcl::make_tracked<Request>(1));
requests.emplace(sgcl::make_tracked<Request>(2));
```

### try_pop, pop

```cpp
optional<T> try_pop();   // sgcl::optional, the alias of std::optional (sgcl/core/aliases.h)
T pop();
```

`try_pop` takes the first element: it walks from the head to the first node whose element is not taken and claims it with a compare-exchange on the node's flag; the element, or nothing when the walk reached the last node. `pop` takes the first element, waiting while the queue is empty.

```cpp
while (auto r = requests.try_pop()) {
    (*r)->handle();
}
sgcl::tracked_ptr next = requests.pop();   // blocks until a push
```

### empty, size

```cpp
bool empty() const noexcept;
size_type size() const noexcept;
```

`empty` walks from the head to the first element; `size` counts the elements not taken, in linear time.

### clear

```cpp
void clear() noexcept;
```

Pops every element there is, destroying each on the calling thread.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

// A pipeline stage: producers push messages, consumers pop them in
// order; the queue is a member of a managed object, and nothing in the
// program frees a node
struct Message {
    int producer, seq;
};

struct Stage {
    sgcl::concurrent_queue<sgcl::tracked_ptr<Message>> inbox;   // inside a managed object: sgcl::
};

int main() {
    sgcl::tracked_ptr stage = sgcl::make_tracked<Stage>();
    sgcl::atomic<int> received = 0, out_of_order = 0;
    sgcl::vector<sgcl::thread> threads;
    for (int p : sgcl::range(4)) {
        threads.emplace_back([&, p] {
            for (int i : sgcl::range(1000)) {
                stage->inbox.emplace(sgcl::make_tracked<Message>(p, i));
            }
        });
        threads.emplace_back([&] {
            int last[4] = {-1, -1, -1, -1};
            for (int i : sgcl::range(1000)) {
                sgcl::tracked_ptr m = stage->inbox.pop();   // FIFO per producer, at every consumer
                out_of_order += m->seq <= last[m->producer];
                last[m->producer] = m->seq;
                ++received;
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    std::cout << received << " messages, " << out_of_order << " out of order\n";
    return received == 4000 && out_of_order == 0 ? 0 : 1;
}
```

The output:

```
4000 messages, 0 out of order
```

## See also

- [concurrent_stack](concurrent_stack.md) for the LIFO counterpart, [concurrent_map](concurrent_map.md) for the ordered map
- [atomic](atomic.md), what the head, the tail and the links are
- [queue](../containers/queue.md), the sequential adapter
- [README: Lock-free containers](README.md#lock-free-containers), [README: The rules](../core/README.md#the-rules)

[sgcl](../README.md) › concurrent

# sgcl::concurrent

```cpp
#include "sgcl/concurrent.h"   // namespace sgcl::concurrent
```

Structures shared by any number of threads: the lock-free containers of the textbooks under the names of `std`
and with the interfaces of `java.util.concurrent` (a queue, a stack, two bounded rings, a sorted map and set, a
hash map and set), a priority queue under a lock, a cache over the hash map, the weak containers and a pool of
interned values for many threads, and a value read by every thread and replaced whole. The module depends on
[core](../core/README.md): the structures stand on its containers and on its [atomic](../core/atomic.md) over a
`tracked_ptr`, which is core's and not this module's; the versions of the [immutable](../immutable/README.md)
containers are published to other threads through `copy_on_write`.

The idea the module rests on is that a collector is the reclamation scheme these algorithms need. The
Michael–Scott queue, the Treiber stack, the skip list of Herlihy and Shavit and the split-ordered list of Shalev
and Shavit are each written as the paper has them; in C++ they come with hazard pointers, epochs or counted
pointers, a scheme that decides when a node another thread may still be reading can be freed, and that scheme is
where the difficulty and the bugs are. Here a node is never reused while a thread holds it, so a link is a
`tracked_ptr` in an `atomic`, a compare-exchange is a compare-exchange, there is no ABA, and a popped or unlinked
node is garbage the collector reclaims. A hazard pointer exists in the library, one per thread inside
`atomic::load` for the length of the load, and nothing in the containers or in a program using them knows of it.

The channel of Go and its `select`, the queue with the synchronization of both ends and coroutines waiting on it,
are a class of the [async](../async/README.md) module built on these structures: its buffer is the ring of
`bounded_queue` and its lists of waiters are `concurrent::queue`s ([channel](../async/channel.md)). The
containers against `std` under a mutex, Go and Java, in numbers, are on [benchmarks](benchmarks.md).

## The rules

1. A structure of the module holds `tracked_ptr`s, so it lives where one may: on a thread's stack or inside a
   managed object, never in `new`/`malloc` memory, a `std` container or a global
   ([The rules](../core/README.md#the-rules), 1). What a whole program shares goes into one managed object held
   by a [root_ptr](../core/root_ptr.md), a global; the default pool of `intern` is one. The iterators of the maps
   and sets and the snapshots of a `copy_on_write` are tracked pointers and live where their structure may.
2. A structure is neither copyable nor movable: a structure shared by threads has one place, and the threads
   reach it by reference or through the managed object that holds it.
3. Every member function may be called from any thread at any time, concurrently with any other, unless the
   class says otherwise: `spsc_queue` has one producer and one consumer. What is lock-free, wait-free or
   linearizable, and what waits, is said on each class page and in the Notes of each function.
4. A `tracked_ptr` may not address an element or a node ([The rules](../core/README.md#the-rules), 4). There is
   no `front()` or `top()` by reference: the element is what `try_pop` returns, a copy is what `try_top` gives.
5. `size()` under concurrent modification is a snapshot of no particular moment, as Java's is; it is exact once
   the other threads are quiet.

### Lock-free containers

- The queues and the stack move an element out of its node or cell on the thread that pops it, into the
  `optional` returned, and destroy it there, as `std::queue::pop` and `std::stack::pop` do. `push`, `emplace`,
  `try_push` and `try_pop` never wait; `pop`, and `push` of a bounded ring, wait for an element or for room: in the
  rings and the priority queue a spin of a few microseconds, then a wait in the kernel.
- The maps and sets (`sorted_map`, `sorted_set`, `map`, `set`) are the one place in the library where an element
  outlives its erasure: the thread that erases cannot know who is reading it, so an element is destroyed by the
  collector with its node, once nothing holds the node. Their lookups are wait-free and write nothing, in `map` and
  `set` once the key's bucket has its dummy node (the first lookup in a bucket without one makes it, lock-free);
  their insertions and erasures are lock-free and linearizable. Iteration is weakly consistent, as Java's: an
  iterator holds its node, is valid whatever the other threads do, skips the elements erased since it passed them
  and may or may not see the ones inserted meanwhile. There is no `operator[]`, `at`, `insert_or_assign` or node
  handle.
- A failed compare-exchange on one contended word backs off before the retry, exponentially, as the stack of
  Herlihy and Shavit does: up to `config::backoff_max` pause instructions ([config](../core/config.md)) at the
  head of the stack, in the lock of `priority_queue` and in `copy_on_write::update`, up to 1024 on the positions of
  `bounded_queue`.
- `priority_queue` is the exception, not lock-free on purpose: a binary heap under a spin-then-park lock, which
  every lock-free priority queue it was measured against lost to.

### Weak containers

- `weak_map`, `weak_set` and `intern` hold their objects by [weak_ptr](../core/weak_ptr.md): an entry whose object
  a cycle has found unreachable is dead, never found and passed over by a walk. Between the object becoming
  unreachable and that cycle the entry is found like any other: the lag of any garbage collector.
- The dead entries are swept by an inserting thread, the one whose insertion brings the count since the last sweep
  to as many as the container has entries (16 at least), one sweep at a time and none waiting, so that an
  insertion stays lock-free; `sweep()` runs one on demand. The sweep is safe under the threads because the
  collector clears a weak cell before the object's slot can be handed out again: an entry seen dead is dead for
  good.
- An entry carries the hash it was placed with: the address, or the contents, are gone with the object.

## Functions

| Function | Header | Description |
|---|---|---|
| [intern_string](intern_string.md) | `intern.h` | the interned string of some characters, from the default pool of strings: a `string_view` or a literal, no string made when the value is known |

## Classes

| Class | Header | Description |
|---|---|---|
| [cache\<Key, T, Hash, KeyEqual\>](cache.md) | `cache.h` | a cache over the hash map bounded by a capacity and a time to live, the least recently used evicted by sampling: `get` wait-free on a fresh entry or none, `put` and `get_or_compute` lock-free |
| [copy_on_write\<T\>](copy_on_write.md) | `copy_on_write.h` | a value read by many threads and replaced whole: one load for an immutable snapshot, a copy and a compare-exchange for a change |
| [intern\<T, Hash, KeyEqual\>](intern.md) | `intern.h` | a pool where equal values share one managed object (Go's `unique`, Java's `String.intern`), held weakly and compared by identity |

## Containers

| Container | Header | Description |
|---|---|---|
| [bounded_queue\<T\>](bounded_queue.md) | `bounded_queue.h` | Vyukov's bounded MPMC queue: a ring of cells with sequence numbers, one compare-exchange per operation, no allocation per element; Go's buffered `chan` without the lock |
| [map\<Key, T, Hash, KeyEqual\>](map.md) | `map.h` | the split-ordered list of Shalev and Shavit: a lock-free hash map that doubles its bucket array without moving a node |
| [priority_queue\<T, Compare\>](priority_queue.md) | `priority_queue.h` | a binary heap under a spin-then-park lock, Java's `PriorityBlockingQueue`: the least element first, equal ones in the order they came |
| [queue\<T\>](queue.md) | `queue.h` | the Michael–Scott queue in the form of Java's `ConcurrentLinkedQueue`: unbounded, FIFO, `push`, `try_pop`, a blocking `pop` |
| [set\<Key, Hash, KeyEqual\>](set.md) | `set.h` | the hash table of `map` with the key as the element |
| [sorted_map\<Key, T, Compare\>](sorted_map.md) | `sorted_map.h` | the lock-free skip list of Herlihy and Shavit, Java's `ConcurrentSkipListMap`: a map in key order |
| [sorted_set\<Key, Compare\>](sorted_set.md) | `sorted_set.h` | the skip list of `sorted_map` with the key as the element |
| [spsc_queue\<T\>](spsc_queue.md) | `spsc_queue.h` | a ring for one producer and one consumer: wait-free, no compare-exchange, the cell the one line the two share |
| [stack\<T\>](stack.md) | `stack.h` | the Treiber stack: one word, `push`, `try_pop`, a blocking `pop`, a backoff after a lost exchange |

## Weak containers

| Container | Header | Description |
|---|---|---|
| [weak_map\<Key, T\>](weak_map.md) | `weak_map.h` | the [weak_map](../core/weak_map.md) shared by any number of threads: values attached to objects it does not keep alive, over the lock-free hash table |
| [weak_set\<Key\>](weak_set.md) | `weak_set.h` | the same table with the objects alone: a set of objects it does not keep alive |

## See also

- [Benchmarks](benchmarks.md): the containers against `std` under a mutex, Go and Java
- [atomic](../core/atomic.md), [atomic_ref](../core/atomic_ref.md): the lock-free atomic `tracked_ptr` the
  structures stand on
- [channel](../async/channel.md), [select](../async/select.md): Go's channel and its `select`, over these structures
- [core: Containers](../core/README.md#containers), [core: Weak containers](../core/README.md#weak-containers): the
  sequential counterparts
- [The modules](../README.md)

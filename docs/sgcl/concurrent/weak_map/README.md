[sgcl](../../README.md) › [concurrent](../README.md)

# sgcl::concurrent::weak_map\<Key, T\>

```cpp
#include "sgcl/concurrent/weak_map.h"   // or "sgcl/concurrent.h"

namespace sgcl::concurrent {
    template<class Key, class T>
    class weak_map;
}
```

`sgcl::concurrent::weak_map<Key, T>` is the [weak_map](../../core/weak_map/README.md) shared by any number of threads without
a lock: a map from objects to values that does not keep the objects alive, over the lock-free hash table of
[concurrent::map](../map/README.md), the split-ordered list of Shalev and Shavit. The key is the object itself, its identity
and not its contents: an entry is looked up, made and erased by a `tracked_ptr<Key>` to the object and held by a
[weak_ptr](../../core/weak_ptr/README.md). An entry whose object the collector has found unreachable is dead: never found,
passed over by the iteration, dropped by a sweep. Metadata attached to objects from several threads, a cache keyed
by the object that the workers share, a registry that forgets. [concurrent::weak_set](../weak_set/README.md) holds the objects
alone.

The entries are hashed and compared by the object's address, which the weak pointer's cell holds while the object
lives and the collector clears before the address can be handed out again; so a dead entry equals nothing, its own
key included, and can neither be found nor block the entry of the object that takes the slot next. One thing differs
from the sequential map: the split-ordered list keeps a node's place from the hash at the insertion and hashes the
key again to erase it, and the address, so the hash, is gone once the object dies; so an entry carries the hash it
was placed with, and is erased from where it was put.

The dead entries are swept by the inserting threads: the insertion that brings the count since the last sweep to as
many as the map had entries after it (16 at least, so that a pass costs less than the insertions that paid for it)
walks the map and erases every entry whose cell is cleared. One sweep runs at a time, and a thread that finds one
under way goes on without waiting, so an insertion never waits for a sweep; [sweep](sweep.md) runs one on
demand.

What differs from `std::unordered_map` and from the sequential `weak_map`: there is no `operator[]`, no `at` and no
`insert_or_assign`, as the table has none: a value is set once, at the insertion, and changed through an
[atomic](../../core/atomic.md) inside it; there is no `const_iterator`, and the map is neither copied nor moved. What
differs from Java's `WeakHashMap`: the map is shared without a lock, where Java's is wrapped in
`Collections.synchronizedMap`; the key is compared by identity, not by `equals`, as in Java's `IdentityHashMap`.
Go's library has no weak map.

## Rules

- The map holds tracked pointers (the table's array of buckets, its head and the counters), so it lives on a
  thread's stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1); the one a program shares
  goes into a managed object under a [root_ptr](../../core/root_ptr/README.md). The values may be, or hold, tracked pointers:
  the nodes are managed objects.
- Every member function may be called from any thread at any time, and none waits. `find`, `contains` and `count`
  are wait-free and write nothing once the object's bucket has its dummy node, which the first lookup or insertion
  in the bucket makes (an allocation and a compare-exchange, lock-free, once per bucket for the array's life).
  `insert`, `emplace`, `try_emplace` and `erase` are lock-free and linearizable at the table's compare-exchange: of
  two threads inserting the same object, exactly one gets `true`. `sweep` and `clear` are walks of lock-free
  erasures.
- Iteration is weakly consistent, as the table's: an iterator holds its node and, on a live entry, the object as a
  strong pointer, so it is valid whatever the other threads do and the entry cannot die under it; it skips the
  entries erased since it passed them and may or may not see the ones inserted meanwhile. An iterator is a tracked
  object then, and lives where the map may.
- An entry is dead once a cycle has found its object unreachable; between the object becoming unreachable and that
  cycle, it is found and visited like any other: the lag of any garbage collector.
- The values are the map's own, destroyed with the node by the collector once nothing holds it, not at the erasure:
  a value stays alive while an iterator holds its node, erased or swept or not, and a reference to it taken through
  an iterator is valid while the iterator exists. A value holding a strong pointer to its own key keeps the key
  alive, and the entry with it: there are no ephemerons.
- `size()` counts the dead entries not yet swept, and under concurrent modification is a snapshot of no particular
  moment ([README: The rules](../README.md#the-rules), 5); `size()` and `empty()` are exact after a `sweep()` with the
  threads quiet.
- Non-copyable, non-movable: a shared structure has one place.

## Template parameters

| Parameter | Description |
|---|---|
| `Key` | The type of the objects: any type a `tracked_ptr` points to, a class, an abstract base, `int`. It is neither hashed nor compared: the key is the object's identity. |
| `T` | The type of the values: any object type constructible from the arguments of the insertion; it need not be copyable or movable, as `try_emplace` constructs it in the node. It may be, or hold, tracked pointers. |

## Member types

| Type | Definition |
|---|---|
| `key_type` | `Key` |
| `key_pointer` | `tracked_ptr<Key>` |
| `mapped_type` | `T` |
| `weak_type` | `weak_ptr<Key>` |
| `size_type` | `size_t` |
| `reference` | `struct { key_pointer key; T& value; }`: what an iterator gives out, the object, held, and its value |
| `iterator` | a forward iterator over the live entries, of a class of the library; `*it` is a `reference`, `it->key`, `it->value` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](weak_map.md) | constructs an empty map |
| `(destructor)` | leaves the nodes, and the values in them, to the collector |

#### Iterators

| Function | Description |
|---|---|
| [begin](begin.md) | an iterator to the first live entry |
| [end](end.md) | an iterator past the last entry |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the map holds an entry, a dead one included |
| [size](size.md) | the number of entries, the dead ones not yet swept included |

#### Modifiers

| Function | Description |
|---|---|
| [clear](clear.md) | erases every entry |
| [insert](insert.md) | inserts a value for an object, unless the object has one |
| [emplace](emplace.md) | the same as `try_emplace` |
| [try_emplace](try_emplace.md) | constructs a value for an object in place, unless the object has one |
| [erase](erase.md) | erases the entry of an object, or the one an iterator stands on |
| [sweep](sweep.md) | erases the entries whose objects are gone |

#### Lookup

| Function | Description |
|---|---|
| [count](count.md) | the number of entries of an object, 0 or 1 |
| [find](find.md) | the entry of an object |
| [contains](contains.md) | checks whether an object has an entry |

## Complexity

- `find`, `contains`, `count`, `erase`: constant on average, a walk of the object's bucket, which holds an entry or
  two: the array of buckets doubles once the entries outnumber the buckets.
- `insert`, `emplace`, `try_emplace`: constant on average, plus, every so many insertions, a sweep linear in the
  number of entries: amortized constant, as the sweep comes after as many insertions as the map had entries.
- `sweep`, `clear`: linear in the number of entries. `size`: constant.
- `empty`, `begin`: constant when an entry stands near the head of the list; at worst linear in the number of
  buckets in use, whose dummy nodes the walk to the first entry passes, and for `begin` in the dead entries before
  the first live one. `end`: constant.

Across sixteen threads a lookup and an insertion take 3.8 ns per operation, against 51 for Java's `WeakHashMap`
under `Collections.synchronizedMap`; on one thread 55 ns against Java's 35, a plain hash map against the
split-ordered list ([Benchmarks: The bounded queue, the priority queue, intern and the weak
map](../benchmarks.md#the-bounded-queue-the-priority-queue-intern-and-the-weak-map)).

## Iterator invalidation

An iterator is never invalidated: it holds its node, and on a live entry its object, as tracked pointers. An entry
erased while an iterator stands on it stays readable through it, its value included, and the iterator walks on from
it to the entries after.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Session {
    int id;
};

struct Stats {
    atomic<int> requests = 0;
};

int main() {
    concurrent::weak_map<Session, Stats> stats;  // shared by the workers
    tracked_ptr user = make_tracked<Session>(1);
    vector<thread> workers;
    for (int t : range(4)) {
        workers.emplace_back([&stats, &user, t] {
            tracked_ptr guest = make_tracked<Session>(100 + t);  // gone with the thread
            for (int i : range(1000)) {
                tracked_ptr<Session> session = i % 2 ? user : guest;
                ++stats.try_emplace(session).first->value.requests;  // one Stats per session
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    println("{} entries", stats.size());

    collector::force_collect(true);  // optional, for the demonstration: the guests are found dead
    for (auto [session, s] : stats) {
        println("session {}: {} requests", session->id, s.requests.load());
    }
    size_t swept = stats.sweep();
    println("{} swept, {} left", swept, stats.size());
}
```

Output:

```text
5 entries
session 1: 2000 requests
4 swept, 1 left
```

## See also

- [concurrent::weak_set](../weak_set/README.md): the objects alone
- [intern](../intern/README.md): a pool keyed by the contents of the objects, over the same weak entries
- [weak_map](../../core/weak_map/README.md): the sequential map, with `operator[]` and `insert_or_assign`
- [weak_ptr](../../core/weak_ptr/README.md): what holds the key
- [concurrent::map](../map/README.md): the hash table underneath, and its rules
- [README: Weak containers](../README.md#weak-containers), [README: Lock-free
  containers](../README.md#lock-free-containers), [core: Weak containers](../../core/README.md#weak-containers)

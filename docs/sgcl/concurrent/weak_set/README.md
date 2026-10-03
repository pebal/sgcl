[sgcl](../../README.md) › [concurrent](../README.md)

# sgcl::concurrent::weak_set\<Key\>

```cpp
#include "sgcl/concurrent/weak_set.h"   // or "sgcl/concurrent.h"

namespace sgcl::concurrent {
    template<class Key>
    class weak_set;
}
```

`sgcl::concurrent::weak_set<Key>` is the [weak_set](../../core/weak_set/README.md) shared by any number of threads without a
lock: a set of objects that does not keep them alive, the [concurrent::weak_map](../weak_map/README.md) of nothing but keys,
over the lock-free hash table of [concurrent::set](../set/README.md), the split-ordered list of Shalev and Shavit. An object
is inserted, found and erased by a `tracked_ptr<Key>` to it and held by a [weak_ptr](../../core/weak_ptr/README.md); an entry
whose object the collector has found unreachable is dead: never found, passed over by the iteration, dropped by a
sweep. Objects registered from several threads without being owned there: the open connections, the listeners, the
instances of a class, a set that forgets.

Hashing, equality and the sweeps are those of `concurrent::weak_map`: the entries are hashed and compared by the
object's address, which the collector clears from the weak pointer's cell before the address can be handed out
again, so a dead entry equals nothing; an entry carries the hash it was placed with, and is erased from where it was
put; and the insertion that brings the count since the last sweep to as many as the set had entries after it (16 at
least) sweeps the dead entries out, one sweep at a time and no thread waiting for it.

What differs from `std::unordered_set`: the element is the object, compared by identity, not by value, and held only
while an iterator stands on it. What differs from the sequential `weak_set`: there is no `const_iterator`, and the
set is neither copied nor moved. What differs from Java, which has no weak set of its own and makes one with
`Collections.newSetFromMap(new WeakHashMap<>())` under `Collections.synchronizedSet`: the set is shared without a
lock, and compares by identity, not by `equals`. Go's library has no weak set.

## Rules

- The set holds tracked pointers (the table's array of buckets, its head and the counters), so it lives on a
  thread's stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1); the one a program shares
  goes into a managed object under a [root_ptr](../../core/root_ptr/README.md).
- Every member function may be called from any thread at any time, and none waits. `find`, `contains` and `count`
  are wait-free and write nothing once the object's bucket has its dummy node, which the first lookup or insertion
  in the bucket makes (an allocation and a compare-exchange, lock-free, once per bucket for the array's life).
  `insert` and `erase` are lock-free and linearizable at the table's compare-exchange: of two threads inserting the
  same object, exactly one gets `true`. `sweep` and `clear` are walks of lock-free erasures.
- Iteration is weakly consistent: an iterator holds its node and, on a live entry, the object as a strong pointer,
  so it is valid whatever the other threads do and the object cannot die under it; it skips the entries erased since
  it passed them and may or may not see the ones inserted meanwhile. An iterator is a tracked object then, and lives
  where the set may.
- An entry is dead once a cycle has found its object unreachable; between the object becoming unreachable and that
  cycle, it is found and visited like any other: the lag of any garbage collector.
- `size()` counts the dead entries not yet swept, and under concurrent modification is a snapshot of no particular
  moment ([README: The rules](../README.md#the-rules), 5); `size()` and `empty()` are exact after a `sweep()` with the
  threads quiet.
- Non-copyable, non-movable: a shared structure has one place.

## Template parameters

| Parameter | Description |
|---|---|
| `Key` | The type of the objects: any type a `tracked_ptr` points to, a class, an abstract base, `int`. It is neither hashed nor compared: the element is the object's identity. |

## Member types

| Type | Definition |
|---|---|
| `key_type` | `Key` |
| `key_pointer` | `tracked_ptr<Key>` |
| `weak_type` | `weak_ptr<Key>` |
| `size_type` | `size_t` |
| `reference` | `key_pointer`: what an iterator gives out, the object, held |
| `iterator` | a forward iterator over the live objects, of a class of the library; `*it` is a `key_pointer`, `(*it)->member` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](weak_set.md) | constructs an empty set |
| `(destructor)` | leaves the nodes to the collector |

#### Iterators

| Function | Description |
|---|---|
| [begin](begin.md) | an iterator to the first live object |
| [end](end.md) | an iterator past the last entry |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the set holds an entry, a dead one included |
| [size](size.md) | the number of entries, the dead ones not yet swept included |

#### Modifiers

| Function | Description |
|---|---|
| [clear](clear.md) | erases every entry |
| [insert](insert.md) | inserts an object, unless the set holds it |
| [erase](erase.md) | erases an object, or the entry an iterator stands on |
| [sweep](sweep.md) | erases the entries whose objects are gone |

#### Lookup

| Function | Description |
|---|---|
| [count](count.md) | the number of entries of an object, 0 or 1 |
| [find](find.md) | the entry of an object |
| [contains](contains.md) | checks whether the set holds an object |

## Complexity

- `find`, `contains`, `count`, `erase`: constant on average, a walk of the object's bucket, which holds an entry or
  two: the array of buckets doubles once the entries outnumber the buckets.
- `insert`: constant on average, plus, every so many insertions, a sweep linear in the number of entries: amortized
  constant, as the sweep comes after as many insertions as the set had entries.
- `sweep`, `clear`: linear in the number of entries. `size`: constant.
- `empty`, `begin`: constant when an entry stands near the head of the list; at worst linear in the number of
  buckets in use, whose dummy nodes the walk to the first entry passes, and for `begin` in the dead entries before
  the first live one. `end`: constant.

## Iterator invalidation

An iterator is never invalidated: it holds its node, and on a live entry its object, as tracked pointers. An entry
erased while an iterator stands on it stays readable through it, and the iterator walks on from it to the entries
after.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Connection {
    int id;
    atomic<int> notices = 0;
};

int main() {
    concurrent::weak_set<Connection> open;  // every open connection, owned elsewhere
    concurrent::queue<tracked_ptr<Connection>> kept;  // the connections that stay open
    vector<thread> acceptors;
    for (int t : range(4)) {
        acceptors.emplace_back([&open, &kept, t] {
            for (int i : range(100)) {
                tracked_ptr c = make_tracked<Connection>(t * 100 + i);
                open.insert(c);
                if (i % 50 == 0) {
                    kept.push(c);  // the rest are closed when their thread drops them
                }
            }
        });
    }
    for (auto& a : acceptors) {
        a.join();
    }

    collector::force_collect(true);  // optional, for the demonstration: the closed ones die
    int reached = 0;
    for (tracked_ptr c : open) {  // the open connections alone, each held
        ++c->notices;
        ++reached;
    }
    println("{} connections reached", reached);

    open.sweep();
    println("{} entries after a sweep", open.size());
}
```

Output:

```text
8 connections reached
8 entries after a sweep
```

## See also

- [concurrent::weak_map](../weak_map/README.md): a value for each object
- [weak_set](../../core/weak_set/README.md): the sequential set
- [weak_ptr](../../core/weak_ptr/README.md): what holds the object
- [concurrent::set](../set/README.md): the hash table underneath, and its rules
- [README: Weak containers](../README.md#weak-containers), [README: Lock-free
  containers](../README.md#lock-free-containers), [core: Weak containers](../../core/README.md#weak-containers)

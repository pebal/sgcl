# sgcl::concurrent_weak_map

```cpp
#include "sgcl/concurrent/concurrent_weak_map.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class Key, class T>
    class concurrent_weak_map;
}
```

The same class in the `Sgcl` interface: [ConcurrentWeakDictionary](../Sgcl/Concurrent/ConcurrentWeakDictionary.md).

`concurrent_weak_map<Key, T>` is the [weak_map](../containers/weak_map.md) shared by any number of threads without a lock: a map from objects to values that does not keep the objects alive, over the lock-free hash table of [concurrent_unordered_map](concurrent_unordered_map.md) (Java's `WeakHashMap` with the concurrency of its `ConcurrentHashMap`). The key is the object itself, its identity and not its contents: an entry is looked up, made and erased by a `tracked_ptr<Key>` to the object and held by a [`weak_ptr`](../core/weak_ptr.md). An entry whose object the collector has found unreachable is dead: never found, passed over by the iteration, dropped by a sweep. Metadata attached to objects from several threads, a cache keyed by the object that the workers share, a registry that forgets. [`concurrent_weak_set`](concurrent_weak_set.md) holds the objects alone.

The entries are hashed and compared by the object's address, as in `weak_map`, which the weak pointer's cell holds while the object lives and the collector clears before the address can be handed out again ([Weak pointers](../core/README.md#weak-pointers)); so a dead entry equals nothing, its own key included, and can neither be found nor block the entry of the object that takes the slot next. One thing differs from the sequential map: the split-ordered list keeps a node's place from the hash at the insertion and hashes the key again to erase by iterator, and the address, so the hash, is gone once the object dies; so an entry carries the hash it was placed with, and is erased from where it was put.

The dead entries are swept by the inserting threads: the insertion that brings the count since the last sweep to the threshold (as many as the map has entries, 16 at least, so that a pass costs less than the insertions that paid for it) walks the map and erases, by iterator, every entry whose cell is cleared. That is safe under concurrent use: the collector clears a cell before the object's slot can be handed out again, so an entry seen dead is dead for good and no thread can find it alive meanwhile, and erasing it races with nothing but another erase of the same node, which the table settles (one thread marks it). One sweep runs at a time: a thread finding one under way goes on without waiting, so an insertion never blocks on a sweep. `sweep()` runs one on demand and returns how many entries it dropped, or 0 at once when another thread's sweep is under way. `size()` is the table's count, the dead entries not yet swept included.

The values are the map's own, destroyed with the node by the collector once nothing holds it: a value holding a strong pointer to its own key keeps the key alive, and the entry with it (there are no ephemerons). There is no `operator[]` and no `insert_or_assign`, as the table has none: a value is set once, at the insertion, and changed through an [`atomic`](atomic.md) inside it.

## Rules

- The map holds tracked pointers (the table's array, head and counters), so it lives on a thread's stack or inside a managed object ([The rules](../core/README.md#the-rules), 1); the one a program shares goes into a managed object under a `root_ptr`. The values may be, or hold, tracked pointers: the nodes are managed objects.
- `find`, `contains` and `count` are wait-free and never write; `insert`, `emplace`, `try_emplace` and `erase` are lock-free and linearizable at the table's compare-exchange. A concurrent insertion of the same object wins or loses there: exactly one returns `true`.
- Iteration is weakly consistent, as the table's: an iterator holds its node and, on a live entry, the object as a strong pointer, so it is valid whatever the other threads do and the entry cannot die under it; it skips the entries erased since it passed them and may or may not see the ones inserted meanwhile. An iterator is a tracked object then, and lives where the map's pointers may.
- An entry is dead once a cycle has found its object unreachable; between the object becoming unreachable and that cycle, it is found and visited like any other: the lag of any garbage collector.
- The value of an entry stays alive for as long as an iterator holds the node, erased or swept or not, as in `concurrent_unordered_map`; a reference to it taken through an iterator is valid while the iterator exists.
- `size()` is the sum of the table's stripes, a snapshot of no particular moment under concurrent modification, exact once the threads are quiet; `empty()` is exact after a `sweep()` with the threads quiet.
- Non-copyable, non-movable: a shared structure has one place.

## Members

### Types

```cpp
using key_type = Key;
using key_pointer = tracked_ptr<Key>;
using mapped_type = T;
using weak_type = weak_ptr<Key>;
using size_type = size_t;
struct reference { key_pointer key; T& value; };   // what an iterator gives out
using iterator = /* forward iterator over the live entries */;
```

`reference` is what `*it` returns: the object, held, and the value; `it->key`, `it->value`. There is no `const_iterator`, and `begin()`/`end()` are not `const`: standing on an entry holds its object, which is a write to the iterator, not to the map.

### Constructors

```cpp
concurrent_weak_map();
concurrent_weak_map(const concurrent_weak_map&) = delete;
```

### begin, end

```cpp
iterator begin() noexcept;
iterator end() noexcept;
```

The live entries, each once, in the order of the table's list (the bit reversal of the addresses' hashes); the dead ones are passed over without being dropped. Weakly consistent.

```cpp
struct Session { int id; };
sgcl::concurrent_weak_map<Session, sgcl::tracked_ptr<Stats>> stats;   // shared by the workers
for (auto [session, s] : stats) {   // session: sgcl::tracked_ptr<Session>, held; s: sgcl::tracked_ptr<Stats>&
    std::cout << session->id << ' ' << s->requests << '\n';
}
```

### find, count, contains

```cpp
iterator find(const key_pointer& object) noexcept;
size_type count(const key_pointer& object) const noexcept;
bool contains(const key_pointer& object) const noexcept;
```

The entry of the object, or `end()`; a null pointer has none, and an object that is gone has none. Wait-free; the iterator holds the node and the object.

### try_emplace, emplace, insert

```cpp
template<class... A> pair<iterator, bool> try_emplace(const key_pointer& object, A&&... a);
template<class... A> pair<iterator, bool> emplace(const key_pointer& object, A&&... a);
pair<iterator, bool> insert(const key_pointer& object, const T& value);
pair<iterator, bool> insert(const key_pointer& object, T&& value);
```

A value for the object, `T(a...)`, unless the object has one: the entry and whether one was added. One search: nothing is built, no weak cell either, when the object has an entry; of two threads inserting the same object exactly one gets `true`, the other the entry the first made. Every insertion counts towards the next sweep. A null pointer is not an object (debug builds assert).

```cpp
auto [it, fresh] = stats.try_emplace(session, sgcl::make_tracked<Stats>());   // one Stats per session, whoever gets there first
++it->value->requests;
```

### erase

```cpp
size_type erase(const key_pointer& object);
iterator erase(iterator pos);
```

The entry of the object, dropped: how many (0 or 1). By iterator: the entry the iterator stands on, if it is still there, and the next live entry. Lock-free; the node is marked, which is where one thread wins when several erase the same entry, then unlinked by a search.

### sweep, clear

```cpp
size_type sweep();
void clear();
```

`sweep()` drops the entries whose objects are gone and returns how many; the inserting threads do it by themselves every so many insertions, and a program that inserts little and wants the memory back calls it. It returns 0 at once when another thread's sweep is under way. `clear()` erases every entry there is at the time of the walk, dead or alive.

### size, empty

```cpp
size_type size() const noexcept;
bool empty() const noexcept;
```

The entries, the dead ones not yet swept included; a snapshot under concurrent modification, exact after a `sweep()` with the threads quiet.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

struct Session {
    explicit Session(int id) : id(id) {}
    int id;
};

// A counter attached to a session from outside, by any thread
struct Stats {
    sgcl::atomic<int> requests = 0;
};

int main() {
    // Statistics per session, shared by the workers: the map holds the
    // sessions weakly, so a session dropped by its owner takes its entry
    // with it, and nobody removes stale ones by hand
    sgcl::concurrent_weak_map<Session, sgcl::tracked_ptr<Stats>> stats;
    sgcl::tracked_ptr main_session = sgcl::make_tracked<Session>(1);
    {
        sgcl::tracked_ptr guest = sgcl::make_tracked<Session>(2);
        sgcl::vector<sgcl::thread> workers;
        for (int t : sgcl::range(4)) {
            workers.emplace_back([&, t] {
                for (int i : sgcl::range(1000)) {
                    sgcl::tracked_ptr<Session> session = (i + t) % 3 ? main_session : guest;
                    auto [it, fresh] = stats.try_emplace(session, sgcl::make_tracked<Stats>());   // one Stats per session, whoever gets there first
                    ++it->value->requests;
                }
            });
        }
        for (auto& w : workers) {
            w.join();
        }
        for (auto [session, s] : stats) {   // session: tracked_ptr<Session>, held; s: tracked_ptr<Stats>&
            std::cout << "session " << session->id << ": " << s->requests << " requests\n";
        }
    }   // the guest's last strong pointer is gone
    sgcl::collector::clear_stack();          // the dead frame zeroed, so that the conservative scan keeps nothing
    sgcl::collector::force_collect(true);    // optional, for the demonstration: the cycle clears the guest's entry
    std::cout << stats.size() << " entries, " << stats.sweep() << " swept, " << stats.size() << " left\n";
}
```

The output (the two sessions in either order):

```
session 1: 2666 requests
session 2: 1334 requests
2 entries, 1 swept, 1 left
```

## See also

- [Benchmarks](benchmarks.md#the-bounded-queue-the-priority-queue-intern-and-the-weak-map): measured against Go and Java

- [concurrent_weak_set](concurrent_weak_set.md): the objects alone; [intern](intern.md): a pool keyed by the contents of the objects, over the same weak entries
- [weak_map](../containers/weak_map.md): the sequential map, with `operator[]` and `insert_or_assign`; [weak_ptr](../core/weak_ptr.md): the key
- [concurrent_unordered_map](concurrent_unordered_map.md): the table underneath, and its rules
- README: [Lock-free containers](README.md#lock-free-containers) (the weak containers and the pool at the end), [Weak containers](../containers/README.md#weak-containers), [The rules](../core/README.md#the-rules)
- `tests/concurrent/concurrent_weak_map.cpp`: every behaviour above, checked, with the threads.

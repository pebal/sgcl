# Sgcl::ConcurrentWeakDictionary

```cpp
#include "sgcl/Sgcl/Concurrent/ConcurrentWeakDictionary.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class Key, class Value>
    class ConcurrentWeakDictionary;
}
```

The same class in the `sgcl` interface: [concurrent_weak_map](../../concurrent/concurrent_weak_map.md).

`ConcurrentWeakDictionary<Key, Value>` is the [WeakDictionary](../Containers/WeakDictionary.md) shared by any number of threads without a lock: a dictionary from objects to values that does not keep the objects alive, over the lock-free hash table of [ConcurrentDictionary](ConcurrentDictionary.md) (Java's `WeakHashMap` with the concurrency of its `ConcurrentHashMap`). The key is the object itself, its identity and not its contents: an entry is looked up, made and removed by a `Ptr<Key>` to the object and held by a [`WeakPtr`](../Core/WeakPtr.md). An entry whose object the collector has found unreachable is dead: never found, passed over by the iteration, dropped by a sweep. Metadata attached to objects from several threads, a cache keyed by the object that the workers share, a registry that forgets. [`ConcurrentWeakHashSet`](ConcurrentWeakHashSet.md) holds the objects alone.

The entries are hashed and compared by the object's address, as in `WeakDictionary`, which the weak pointer's cell holds while the object lives and the collector clears before the address can be handed out again ([Weak pointers](../../core/README.md#weak-pointers)); so a dead entry equals nothing, its own key included, and can neither be found nor block the entry of the object that takes the slot next. An entry carries the hash it was placed with, since the address, so the hash, is gone once the object dies and the table erases by it.

The dead entries are swept by the inserting threads: the insertion that brings the count since the last sweep to the threshold (as many as the dictionary has entries, 16 at least, so that a pass costs less than the insertions that paid for it) walks the dictionary and removes every entry whose cell is cleared. That is safe under concurrent use: the collector clears a cell before the object's slot can be handed out again, so an entry seen dead is dead for good, and removing it races with nothing but another removal of the same node, which the table settles. One sweep runs at a time; a thread finding one under way goes on without waiting, so an insertion never blocks on a sweep. `Sweep()` runs one on demand and returns how many entries it dropped, or 0 at once when another thread's sweep is under way. `Count()` is the table's count, the dead entries not yet swept included.

The lookups are those of `ConcurrentDictionary`, because a raw pointer into a node another thread may remove would not be safe: `TryGet` hands back a copy of the value in an `Optional`, `Find` and `GetOrAdd` an iterator, which holds the node and the object, so `it->value` stays valid for as long as the iterator exists. The values are the dictionary's own, destroyed with the node by the collector once nothing holds it: a value holding a strong pointer to its own key keeps the key alive, and the entry with it (there are no ephemerons). There is no `operator[]` and no `Set`, as the table has none: a value is set once, at the insertion, and changed through an [`Atomic`](Atomic.md) inside it.

## Rules

- The dictionary holds tracked pointers, so it lives on a thread's stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1); the one a program shares goes into a managed object under a `RootPtr`. The values may be, or hold, tracked pointers: the nodes are managed objects.
- `Find`, `TryGet` and `ContainsKey` are wait-free and never write; `Add`, `Emplace`, `GetOrAdd` and `Remove` are lock-free and linearizable at the table's compare-exchange. Of two threads adding the same object exactly one gets `true`.
- Iteration is weakly consistent: an iterator holds its node and, on a live entry, the object as a strong pointer, so it is valid whatever the other threads do and the entry cannot die under it; it skips the entries removed since it passed them and may or may not see the ones added meanwhile.
- An entry is dead once a cycle has found its object unreachable; between the object becoming unreachable and that cycle, it is found and visited like any other: the lag of any garbage collector.
- `Count()` is a snapshot under concurrent modification, exact once the threads are quiet; `IsEmpty()` is exact after a `Sweep()` with the threads quiet.
- Non-copyable, non-movable: a shared structure has one place.

## Members

### Types

```cpp
using KeyType = Ptr<Key>;
using ValueType = Value;
using InnerType = sgcl::concurrent_weak_map<Key, Value>;
using SizeType = size_t;
using Iterator = InnerType::iterator;               // forward, over the live entries; holds the node and the object
```

`*it` is `{key, value}`: the object, held (as a `sgcl::tracked_ptr<Key>`), and the value; `it->key`, `it->value`. There is no const iterator, and the free `begin`/`end` are not `const`: standing on an entry holds its object, which is a write to the iterator, not to the dictionary.

### Constructors

```cpp
ConcurrentWeakDictionary();
ConcurrentWeakDictionary(const ConcurrentWeakDictionary&) = delete;
```

### begin, end, End

```cpp
Iterator begin(ConcurrentWeakDictionary&) noexcept;   // free functions
Iterator end(ConcurrentWeakDictionary&) noexcept;
Iterator End() noexcept;
```

The live entries, each once, in no particular order; the dead ones are passed over without being dropped. Weakly consistent.

```cpp
struct Session { int id; };
ConcurrentWeakDictionary<Session, Ptr<Stats>> stats;   // shared by the workers
for (auto [session, s] : stats) {   // session: sgcl::tracked_ptr<Session>, held; s: Ptr<Stats>&
    std::cout << session->id << ' ' << s->requests << '\n';
}
```

### Add, Emplace, GetOrAdd

```cpp
bool Add(const Ptr<Key>& object, const Value& value);
bool Add(const Ptr<Key>& object, Value&& value);
template<class... A> bool Emplace(const Ptr<Key>& object, A&&... a);
template<class... A> Iterator GetOrAdd(const Ptr<Key>& object, A&&... a);
```

A value for the object, `Value(a...)`, unless the object has one: whether one was added, or the entry as an iterator. The object is looked up first and nothing is built when it is there; of two threads adding the same object exactly one gets `true`, the other the entry the first made. Every insertion counts towards the next sweep. A null pointer is not an object (debug builds assert).

```cpp
auto it = stats.GetOrAdd(session, Make<Stats>());   // one Stats per session, whoever gets there first
++it->value->requests;
```

### TryGet, Find, ContainsKey

```cpp
Optional<Value> TryGet(const Ptr<Key>& object);
Iterator Find(const Ptr<Key>& object) noexcept;
bool ContainsKey(const Ptr<Key>& object) const noexcept;
```

A copy of the object's value, or `None`; the entry as an iterator, `End()` when absent. A null pointer has no entry, and an object that is gone has none. Wait-free.

### Remove

```cpp
bool Remove(const Ptr<Key>& object);
Iterator Remove(Iterator pos);
```

The entry of the object, dropped: whether there was one. By iterator: the entry the iterator stands on, if it is still there, and the next live entry. Lock-free.

### Sweep, Clear

```cpp
SizeType Sweep();
void Clear();
```

`Sweep()` drops the entries whose objects are gone and returns how many; the inserting threads do it by themselves every so many insertions, and a program that inserts little and wants the memory back calls it. It returns 0 at once when another thread's sweep is under way. `Clear()` removes every entry there is at the time of the walk, dead or alive.

### Count, IsEmpty, Inner

```cpp
SizeType Count() const noexcept;
bool IsEmpty() const noexcept;
InnerType& Inner() noexcept;
```

The entries, the dead ones not yet swept included; `Inner()` is the `sgcl::concurrent_weak_map` underneath.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

struct Session {
    explicit Session(int id) : id(id) {}
    int id;
};

// A counter attached to a session from outside, by any thread
struct Stats {
    Atomic<int> requests = 0;
};

int main() {
    // Statistics per session, shared by the workers: the dictionary holds
    // the sessions weakly, so a session dropped by its owner takes its
    // entry with it, and nobody removes stale ones by hand
    ConcurrentWeakDictionary<Session, Ptr<Stats>> stats;
    Ptr main_session = Make<Session>(1);
    {
        Ptr guest = Make<Session>(2);
        List<Thread> workers;
        for (int t : Range(4)) {
            workers.Emplace([&, t] {
                for (int i : Range(1000)) {
                    Ptr<Session> session = (i + t) % 3 ? main_session : guest;
                    auto it = stats.GetOrAdd(session, Make<Stats>());   // one Stats per session, whoever gets there first
                    ++it->value->requests;
                }
            });
        }
        for (auto& w : workers) {
            w.Join();
        }
        for (auto [session, s] : stats) {   // session: sgcl::tracked_ptr<Session>, held; s: Ptr<Stats>&
            std::cout << "session " << session->id << ": " << s->requests << " requests\n";
        }
    }   // the guest's last strong pointer is gone
    Collector::ClearStack();                 // the dead frame zeroed, so that the conservative scan keeps nothing
    Collector::Collect(true);                // optional, for the demonstration: the cycle clears the guest's entry
    std::cout << stats.Count() << " entries, " << stats.Sweep() << " swept, " << stats.Count() << " left\n";
}
```

The output (the two sessions in either order):

```
session 2: 1334 requests
session 1: 2666 requests
2 entries, 1 swept, 1 left
```

## See also

- [ConcurrentWeakHashSet](ConcurrentWeakHashSet.md): the objects alone; [Intern](Intern.md): a pool keyed by the contents of the objects, over the same weak entries
- [WeakDictionary](../Containers/WeakDictionary.md): the sequential dictionary, with `operator[]` and `Set`; [WeakPtr](../Core/WeakPtr.md): the key
- [ConcurrentDictionary](ConcurrentDictionary.md): the table underneath, and its rules
- README: [Lock-free containers](../../concurrent/README.md#lock-free-containers), [The rules](../../core/README.md#the-rules)
- `tests/Sgcl/weak_and_intern.cpp`: the wrapper checked; `tests/concurrent/concurrent_weak_map.cpp`: every behaviour above, with the threads.

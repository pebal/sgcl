# Sgcl::ConcurrentWeakHashSet

```cpp
#include "sgcl/Sgcl/Concurrent/ConcurrentWeakDictionary.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class Key>
    class ConcurrentWeakHashSet;
}
```

The same class in the `sgcl` interface: [concurrent_weak_set](../../concurrent/concurrent_weak_set.md).

`ConcurrentWeakHashSet<Key>` is the [WeakHashSet](../Containers/WeakHashSet.md) shared by any number of threads without a lock: a set of objects that does not keep them alive, the [`ConcurrentWeakDictionary`](ConcurrentWeakDictionary.md) of nothing but keys, over the lock-free hash table of [ConcurrentHashSet](ConcurrentHashSet.md). An object is added, found and removed by a `Ptr<Key>` to it and held by a [`WeakPtr`](../Core/WeakPtr.md); an entry whose object the collector has found unreachable is dead: never found, passed over by the iteration, dropped by a sweep, which the inserting threads run by themselves every so many insertions. Objects registered from several threads without being owned there: the open connections, the listeners, the instances of a class, a set that forgets. Hashing, equality, the sweeps and the rules are those of `ConcurrentWeakDictionary`.

## Members

```cpp
using ValueType = Ptr<Key>;
using InnerType = sgcl::concurrent_weak_set<Key>;
using SizeType = size_t;
using Iterator = InnerType::iterator;        // forward, over the live objects; holds the node and the object

ConcurrentWeakHashSet();
ConcurrentWeakHashSet(const ConcurrentWeakHashSet&) = delete;

Iterator begin(ConcurrentWeakHashSet&) noexcept;   // free functions: the live objects, each once; weakly consistent
Iterator end(ConcurrentWeakHashSet&) noexcept;
Iterator End() noexcept;
bool Add(const Ptr<Key>& object);            // whether it was added: of two threads, exactly one gets true
bool Contains(const Ptr<Key>& object) const noexcept;   // wait-free
Iterator Find(const Ptr<Key>& object) noexcept;         // the entry, End() when absent
bool Remove(const Ptr<Key>& object);         // lock-free
Iterator Remove(Iterator pos);
SizeType Sweep();                            // drops the dead entries: how many; 0 at once when another thread's sweep is under way
void Clear();
SizeType Count() const noexcept;             // the dead ones not yet swept included
bool IsEmpty() const noexcept;
InnerType& Inner() noexcept;
```

`*it` is the object as a `sgcl::tracked_ptr<Key>`, held while the iterator stands on it; `it->` is the pointer's own `->`, so `(*it)->member`. A null pointer is not an object: `Add` asserts in debug builds, the lookups find nothing.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

struct Connection {
    explicit Connection(int id) : id(id) {}
    int id;
    Atomic<int> notices = 0;
};

int main() {
    // Every open connection, registered by the thread that accepts it and
    // owned there alone: a connection closed is gone from the set by
    // itself, and a broadcast reaches the ones still open
    ConcurrentWeakHashSet<Connection> open;
    ConcurrentQueue<Ptr<Connection>> kept;   // the connections still open, handed to main
    List<Thread> acceptors;
    for (int t : Range(4)) {
        acceptors.Emplace([&, t] {
            for (int i : Range(100)) {
                Ptr c = Make<Connection>(t * 100 + i);
                open.Add(c);
                if (i % 50 == 0) {
                    kept.Enqueue(c);         // stays open; the rest are closed when the iteration ends
                }
            }
        });
    }
    for (auto& a : acceptors) {
        a.Join();
    }
    Collector::Collect(true);                // optional, for the demonstration: the closed connections found unreachable
    int reached = 0;
    for (auto c : open) {                    // sgcl::tracked_ptr<Connection>, held: the open ones only
        ++c->notices;
        ++reached;
    }
    std::cout << reached << " connections reached, " << open.Count() << " entries, " << open.Sweep() << " swept, " << open.Count() << " left\n";
}
```

The output:

```
8 connections reached, 400 entries, 392 swept, 8 left
```

The 400 entries before the sweep: the inserting threads swept at 16, 32, 64, 128 and 256 insertions, while every connection was still open, and found nothing to drop.

## See also

- [ConcurrentWeakDictionary](ConcurrentWeakDictionary.md): the rules; [WeakHashSet](../Containers/WeakHashSet.md): the sequential set; [WeakPtr](../Core/WeakPtr.md)
- [ConcurrentHashSet](ConcurrentHashSet.md): the table underneath
- README: [Lock-free containers](../../concurrent/README.md#lock-free-containers)
- `tests/Sgcl/weak_and_intern.cpp`: the wrapper checked; `tests/concurrent/concurrent_weak_map.cpp`: the set's behaviour, with the threads.

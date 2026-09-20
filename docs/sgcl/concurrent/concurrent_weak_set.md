# sgcl::concurrent_weak_set

```cpp
#include "sgcl/concurrent/concurrent_weak_set.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class Key>
    class concurrent_weak_set;
}
```

`concurrent_weak_set<Key>` is the [weak_set](../containers/weak_set.md) shared by any number of threads without a lock: a set of objects that does not keep them alive, the [`concurrent_weak_map`](concurrent_weak_map.md) of nothing but keys, over the lock-free hash table of [concurrent_sorted_set](concurrent_sorted_set.md). An object is inserted, found and erased by a `tracked_ptr<Key>` to it and held by a [`weak_ptr`](../core/weak_ptr.md); an entry whose object the collector has found unreachable is dead: never found, passed over by the iteration, dropped by a sweep, which the inserting threads run by themselves every so many insertions. Objects registered from several threads without being owned there: the open connections, the listeners, the instances of a class, a set that forgets. Hashing, equality, the sweeps and the rules are those of `concurrent_weak_map`.

## Members

```cpp
using key_type = Key;
using key_pointer = tracked_ptr<Key>;
using weak_type = weak_ptr<Key>;
using size_type = size_t;
using reference = key_pointer;               // what an iterator gives out: the object, held
using iterator = /* forward iterator over the live objects */;

concurrent_weak_set();
concurrent_weak_set(const concurrent_weak_set&) = delete;

iterator begin() noexcept;                   // the live objects, each once; weakly consistent
iterator end() noexcept;
iterator find(const key_pointer& object) noexcept;         // wait-free
size_type count(const key_pointer& object) const noexcept;
bool contains(const key_pointer& object) const noexcept;
pair<iterator, bool> insert(const key_pointer& object);    // whether it was added: of two threads, exactly one gets true
size_type erase(const key_pointer& object);                // lock-free
iterator erase(iterator pos);
size_type sweep();                           // drops the dead entries: how many; 0 at once when another thread's sweep is under way
void clear();
size_type size() const noexcept;             // the dead ones not yet swept included
bool empty() const noexcept;
```

`*it` is the object as a `key_pointer`, held while the iterator stands on it; `it->` is the pointer's own `->`, so `(*it)->member`. A null pointer is not an object: `insert` asserts in debug builds, the lookups find nothing.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

struct Connection {
    explicit Connection(int id) : id(id) {}
    int id;
    atomic<int> notices = 0;
};

int main() {
    // Every open connection, registered by the thread that accepts it and
    // owned there alone: a connection closed is gone from the set by
    // itself, and a broadcast reaches the ones still open
    concurrent_weak_set<Connection> open;
    concurrent_queue<tracked_ptr<Connection>> kept;   // the connections still open, handed to main
    vector<thread> acceptors;
    for (int t : range(4)) {
        acceptors.emplace_back([&, t] {
            for (int i : range(100)) {
                tracked_ptr c = make_tracked<Connection>(t * 100 + i);
                open.insert(c);
                if (i % 50 == 0) {
                    kept.push(c);            // stays open; the rest are closed when the iteration ends
                }
            }
        });
    }
    for (auto& a : acceptors) {
        a.join();
    }
    collector::force_collect(true);    // optional, for the demonstration: the closed connections found unreachable
    int reached = 0;
    for (auto c : open) {                    // tracked_ptr<Connection>, held: the open ones only
        ++c->notices;
        ++reached;
    }
    std::cout << reached << " connections reached, " << open.size() << " entries, " << open.sweep() << " swept, " << open.size() << " left\n";
}
```

The output:

```
8 connections reached, 400 entries, 392 swept, 8 left
```

The 400 entries before the sweep: the inserting threads swept at 16, 32, 64, 128 and 256 insertions, while every connection was still open, and found nothing to drop.

## See also

- [concurrent_weak_map](concurrent_weak_map.md): the rules; [weak_set](../containers/weak_set.md): the sequential set; [weak_ptr](../core/weak_ptr.md)
- [concurrent_sorted_set](concurrent_sorted_set.md): the table underneath
- README: [Lock-free containers](README.md#lock-free-containers), [Weak containers](../containers/README.md#weak-containers)
- `tests/concurrent/concurrent_weak_map.cpp`: the set's behaviour, checked with the map's.

# sgcl::copy_on_write

```cpp
#include "sgcl/concurrent/copy_on_write.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class copy_on_write;
}
```

The same class in the `Sgcl` interface: [CopyOnWrite](../Sgcl/Concurrent/CopyOnWrite.md).

`sgcl::copy_on_write<T>` holds a value read by many threads and replaced by few, whole: the copy-on-write of Java's `CopyOnWriteArrayList`, for any copyable `T`. The value lives in a managed object of its own and is never modified there. A reader loads the pointer, one atomic load, and has an immutable snapshot that stays what it is, and alive, for as long as the reader holds it; a writer copies the value, changes the copy and swings the pointer with a compare-exchange, and the value it replaced is garbage once the last snapshot of it is dropped. No lock on either side, no reference count on the snapshot, no reader ever waits and no writer ever waits for a reader: what an RCU, or a `shared_ptr` swapped under a lock, is built to approximate, in three words of code, because the collector answers the one question those exist for, when the old value may be freed ([README: Lock-free containers](README.md#lock-free-containers)). Configuration, routing tables, lists of listeners, anything read on every request and changed once in a while.

## Rules

- The container is one word, the atomic pointer. `sgcl::copy_on_write` lives where a `tracked_ptr` may: on a thread's stack or inside a managed object ([The rules](../core/README.md#the-rules), 1). A snapshot is a `tracked_ptr<const T>` and lives where one may.
- `load` is one atomic load, wait-free. `store` and `update` are lock-free: a writer that loses the exchange to another writer copies again, so `update`'s function may run more than once, on copies nobody else sees. Writers are meant to be rare next to readers; with many concurrent writers of a large value a mutex around them serializes the copies cheaper.
- `T` is copied on every `update` and constructed on every `store`: the cost of a write is the copy. A snapshot is never modified: `T` is reached as `const` through it.
- A value's destructor runs when the collector reclaims it, once no snapshot holds it.
- Non-copyable, non-movable: a shared value has one place.

## Members

### Types

```cpp
using value_type = T;
using snapshot = tracked_ptr<const T>;   // tracked_ptr<const T>, sgcl::tracked_ptr<const T> for sgcl::copy_on_write
```

### Constructors

```cpp
copy_on_write();                                              // T()
explicit copy_on_write(const T& value);
explicit copy_on_write(T&& value);
template<class... A> explicit copy_on_write(std::in_place_t, A&&... a);
copy_on_write(const copy_on_write&) = delete;
```

```cpp
sgcl::copy_on_write<Config> config(std::in_place, "localhost", 8080);   // a global: sgcl::
sgcl::copy_on_write<sgcl::vector<sgcl::tracked_ptr<Listener>>> listeners;
```

### load

```cpp
snapshot load() const noexcept;
operator snapshot() const noexcept;
```

The current value: one atomic load. The snapshot holds the value alive and unchanged for as long as it exists, whatever the writers do meanwhile.

```cpp
auto c = config.load();
connect(c->host, c->port);        // the same value in both, whoever stores meanwhile
```

### store, operator=

```cpp
void store(const T& value);
void store(T&& value);
copy_on_write& operator=(const T& value);
copy_on_write& operator=(T&& value);
```

Replaces the value, whole.

### update

```cpp
template<class F> snapshot update(F&& f);
```

Changes the value: `f(T&)` on a copy of the current one, which then replaces it with a compare-exchange; when another writer got in between, the copy is made and `f` called again, after the backoff of the [stack](concurrent_stack.md) (sixteen writers at one value: 500 to 600 ns per update without it, 20 with, measured), so `f` should be a pure change of its argument. Returns a snapshot of the value installed.

```cpp
listeners.update([&](auto& v) { v.push_back(listener); });
auto after = config.update([](Config& c) { ++c.generation; });
```

### compare_exchange

```cpp
bool compare_exchange(snapshot& expected, const T& desired);
bool compare_exchange(snapshot& expected, T&& desired);
```

Replaces the value with `desired` if the current one is still the one `expected` is a snapshot of, returning `true`; otherwise leaves it and sets `expected` to the current snapshot, returning `false`. `update` written out, for a change decided by the caller.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

// A routing table read on every request by many threads and replaced
// by one: readers take a snapshot and never see a half-changed table,
// the writer never waits for a reader, and no reader waits for anything
struct Route {
    int prefix, next_hop;
};

int main() {
    sgcl::copy_on_write<sgcl::vector<Route>> table(sgcl::vector<Route>{{1, 10}, {2, 20}});
    sgcl::atomic stop = false;
    sgcl::atomic<long> lookups = 0, inconsistent = 0;
    sgcl::vector<sgcl::thread> readers;
    for (int r : sgcl::range(8)) {
        readers.emplace_back([&] {
            while (!stop) {
                auto t = table.load();                    // one load: the table as it was, for as long as t lives
                int hops = 0;
                for (auto& route : *t) {
                    hops += route.next_hop;
                }
                inconsistent += hops != 10 * int(t->size()) * (int(t->size()) + 1) / 2;   // 10 + 20 + ... : whole or nothing
                ++lookups;
            }
        });
    }
    for (int i : sgcl::range(3, 101)) {
        table.update([i](auto& t) { t.push_back({i, 10 * i}); });   // a copy with one more route, swapped in
    }
    stop = true;
    for (auto& r : readers) {
        r.join();
    }
    std::cout << lookups << " lookups, " << inconsistent << " inconsistent, " << table.load()->size() << " routes\n";
    return inconsistent == 0 && table.load()->size() == 100 ? 0 : 1;
}
```

The output of one run (the lookups depend on how the threads interleave):

```
5081 lookups, 0 inconsistent, 100 routes
```

## See also

- [atomic](atomic.md), what the pointer is; [vector](../containers/vector.md) and the other containers as values
- [concurrent_map](concurrent_map.md), [concurrent_unordered_map](concurrent_unordered_map.md) for a value changed in place by many threads
- [README: Lock-free containers](README.md#lock-free-containers), [README: The rules](../core/README.md#the-rules)

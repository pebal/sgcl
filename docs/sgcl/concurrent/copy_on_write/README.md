[sgcl](../../README.md) › [concurrent](../README.md)

# sgcl::concurrent::copy_on_write\<T\>

```cpp
#include "sgcl/concurrent/copy_on_write.h"   // or "sgcl/concurrent.h"

namespace sgcl::concurrent {
    template<class T>
    class copy_on_write;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::concurrent::copy_on_write<T>` holds a value read by many threads and replaced by few, whole: the
copy-on-write of Java's `CopyOnWriteArrayList`, for any copyable `T`. The value lives in a managed object of its
own and is never modified there. A reader loads the pointer, one atomic load, and has an immutable snapshot that
stays what it is, and alive, for as long as the reader holds it; a writer copies the value, changes the copy and
swings the pointer with a compare-exchange, and the value it replaced is garbage once the last snapshot of it is
dropped.

No lock on either side, no reference count on the snapshot, no reader ever waits and no writer ever waits for a
reader: what an RCU, or a `shared_ptr` swapped under a lock, is built to approximate, because the collector
answers the one question those exist for, when the old value may be freed
([README: Lock-free containers](../README.md#lock-free-containers)). Configuration, routing tables, lists of
listeners, anything read on every request and changed once in a while; the versions of the
[immutable](../../immutable/README.md) containers are published to other threads this way.

## Rules

- The container is one word, the atomic pointer. A snapshot is a `tracked_ptr<const T>`.
- Every member function may be called from any thread at any time. `load` is one atomic load, wait-free. `store`,
  `operator=`, `update` and `compare_exchange` are lock-free: a writer that loses the exchange to another writer
  copies again, so `update`'s function may run more than once, on copies nobody else sees. Writers are meant to be
  rare next to readers; with many concurrent writers of a large value a mutex around them serializes the copies
  cheaper.
- `T` is copied on every `update` and constructed on every `store`: the cost of a write is the copy. A snapshot is
  never modified: `T` is reached as `const` through it.
- A value's destructor runs when the collector reclaims it, once no snapshot holds it.
- Non-copyable, non-movable: a shared value has one place.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the value: any copy-constructible object type (a `static_assert` says so). |

## Member types

| Type | Definition |
|---|---|
| `value_type` | `T` |
| `snapshot` | `tracked_ptr<const T>`: the value, immutable and held alive |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](copy_on_write.md) | constructs the first value |
| `(destructor)` | leaves the value to the collector, which destroys it once no snapshot holds it |
| [operator=](operator_assign.md) | replaces the value, whole |

#### Reads

| Function | Description |
|---|---|
| [load, operator snapshot](load.md) | a snapshot of the current value: one atomic load |

#### Writes

| Function | Description |
|---|---|
| [store](store.md) | replaces the value, whole |
| [update](update.md) | changes a copy of the value and swings it in |
| [compare_exchange](compare_exchange.md) | replaces the value if it is still the one a snapshot holds |

## Deduction guides

```cpp
template<class T>
copy_on_write(T) -> copy_on_write<T>;
```

## Complexity

- `load`: constant, one atomic load.
- `store`, `operator=`, `compare_exchange`: the construction of one `T` and one exchange.
- `update`: the copy of the value and the call of the function, once per attempt; an attempt is lost only to
  another writer.

A read across fifteen readers costs 3 ns while a writer replaces the value as fast as it can, against 30 for an
atomic `shared_ptr` and 89 under a `std::shared_mutex`
([Benchmarks: Concurrent containers](../benchmarks.md#concurrent-containers)).

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Route {
    int prefix, next_hop;
};

int main() {
    concurrent::copy_on_write table(vector<Route>{{1, 10}, {2, 20}});
    atomic stop = false;
    atomic<long> lookups = 0, inconsistent = 0;
    vector<thread> readers;
    for (int r : range(8)) {
        readers.emplace_back([&] {
            while (!stop) {
                auto t = table.load();  // the table as it was, for as long as t lives
                int hops = 0;
                for (auto& route : *t) {
                    hops += route.next_hop;
                }
                int n = int(t->size());
                inconsistent += hops != 10 * n * (n + 1) / 2;  // whole or nothing
                ++lookups;
            }
        });
    }
    for (int i : range(3, 101)) {
        table.update([i](vector<Route>& t) { t.push_back({i, 10 * i}); });
    }
    stop = true;
    for (auto& r : readers) {
        r.join();
    }
    println("{} lookups, {} inconsistent", lookups.load(), inconsistent.load());
    println("{} routes", table.load()->size());
}
```

Sample output:

```text
3179 lookups, 0 inconsistent
100 routes
```

## See also

- [atomic](../../core/atomic.md): what the pointer is; a `tracked_ptr` or a handle replaced whole needs no copy
- [map](../map/README.md), [sorted_map](../sorted_map/README.md): values changed in place by many threads, by key
- [cache](../cache/README.md): values looked up by key, bounded
- [README: Lock-free containers](../README.md#lock-free-containers)

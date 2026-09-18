# Sgcl::CopyOnWrite

```cpp
#include "sgcl/Sgcl/Concurrent/CopyOnWrite.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T>
    class CopyOnWrite;
}
```

The same class in the `sgcl` interface: [copy_on_write](../../concurrent/copy_on_write.md).

`CopyOnWrite<T>` holds a value read by many threads and replaced by few, whole: the copy-on-write of Java's `CopyOnWriteArrayList`, for any copyable `T`. The value lives in a managed object of its own and is never modified there. A reader loads the pointer, one atomic load, and has an immutable snapshot that stays what it is, and alive, for as long as the reader holds it; a writer copies the value, changes the copy and swings the pointer with a compare-exchange, and the value it replaced is garbage once the last snapshot of it is dropped. No lock on either side, no reference count on the snapshot, no reader ever waits and no writer ever waits for a reader: what an RCU, or a `shared_ptr` swapped under a lock, is built to approximate, in three words of code, because the collector answers the one question those exist for, when the old value may be freed ([README: Lock-free containers](../../concurrent/README.md#lock-free-containers)). Configuration, routing tables, lists of listeners, anything read on every request and changed once in a while.

## Rules

- The container is one word, the atomic pointer. `CopyOnWrite` lives where a `Ptr` may: on a thread's stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1). A snapshot is a `Ptr<const T>` and lives where one may.
- `Load` is one atomic load, wait-free. `Store` and `Update` are lock-free: a writer that loses the exchange to another writer copies again, so `Update`'s function may run more than once, on copies nobody else sees. Writers are meant to be rare next to readers; with many concurrent writers of a large value a mutex around them serializes the copies cheaper.
- `T` is copied on every `Update` and constructed on every `Store`: the cost of a write is the copy. A snapshot is never modified: `T` is reached as `const` through it.
- A value's destructor runs when the collector reclaims it, once no snapshot holds it.
- Non-copyable, non-movable: a shared value has one place.

## Members

### Types

```cpp
using ValueType = T;
using Snapshot = Ptr<const T>;
using InnerType = sgcl::copy_on_write<T>;
```

### Constructors

```cpp
CopyOnWrite();                                              // T()
explicit CopyOnWrite(const T& value);
explicit CopyOnWrite(T&& value);
template<class... A> explicit CopyOnWrite(std::in_place_t, A&&... a);
CopyOnWrite(const CopyOnWrite&) = delete;
```

```cpp
struct Config { String host; int port; };
struct Listener {};
CopyOnWrite<Config> config(std::in_place, "localhost", 8080);
CopyOnWrite<List<Ptr<Listener>>> listeners;
```

### Load

```cpp
Snapshot Load() const noexcept;
operator Snapshot() const noexcept;
```

The current value: one atomic load. The snapshot holds the value alive and unchanged for as long as it exists, whatever the writers do meanwhile.

```cpp
struct Config { String host; int port; };
CopyOnWrite<Config> config(std::in_place, "localhost", 8080);
auto c = config.Load();
std::cout << c->host << ':' << c->port;   // the same value in both, whoever stores meanwhile
```

### Store, operator=

```cpp
void Store(const T& value);
void Store(T&& value);
CopyOnWrite& operator=(const T& value);
CopyOnWrite& operator=(T&& value);
```

Replaces the value, whole.

### Update

```cpp
template<class F> Snapshot Update(F&& f);
```

Changes the value: `f(T&)` on a copy of the current one, which then replaces it with a compare-exchange; when another writer got in between, the copy is made and `f` called again, so `f` should be a pure change of its argument. Returns a snapshot of the value installed.

```cpp
struct Config { int generation = 0; };
struct Listener {};
CopyOnWrite<Config> config;
CopyOnWrite<List<Ptr<Listener>>> listeners;
Ptr listener = Make<Listener>();
listeners.Update([&](auto& v) { v.Add(listener); });
auto after = config.Update([](Config& c) { ++c.generation; });
```

### CompareExchange

```cpp
bool CompareExchange(Snapshot& expected, const T& desired);
bool CompareExchange(Snapshot& expected, T&& desired);
```

Replaces the value with `desired` if the current one is still the one `expected` is a snapshot of, returning `true`; otherwise leaves it and sets `expected` to the current snapshot, returning `false`. `Update` written out, for a change decided by the caller.

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The word inside, as its own type.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// A routing table read on every request by many threads and replaced
// by one: readers take a snapshot and never see a half-changed table,
// the writer never waits for a reader, and no reader waits for anything
struct Route {
    int prefix, nextHop;
};

int main() {
    CopyOnWrite<List<Route>> table(List<Route>{{1, 10}, {2, 20}});
    Atomic stop = false;
    Atomic<long> lookups = 0, inconsistent = 0;
    List<Thread> readers;
    for (int r : Range(8)) {
        readers.Emplace([&] {
            while (!stop) {
                auto t = table.Load();                    // one load: the table as it was, for as long as t lives
                int hops = 0;
                for (auto& route : *t) {
                    hops += route.nextHop;
                }
                inconsistent += hops != 10 * int(t->Count()) * (int(t->Count()) + 1) / 2;   // 10 + 20 + ... : whole or nothing
                ++lookups;
            }
        });
    }
    for (int i : Range(3, 101)) {
        table.Update([i](auto& t) { t.Add({i, 10 * i}); });   // a copy with one more route, swapped in
    }
    stop = true;
    for (auto& r : readers) {
        r.Join();
    }
    std::cout << lookups << " lookups, " << inconsistent << " inconsistent, " << table.Load()->Count() << " routes\n";
    return inconsistent == 0 && table.Load()->Count() == 100 ? 0 : 1;
}
```

The output of one run (the lookups depend on how the threads interleave):

```
5992 lookups, 0 inconsistent, 100 routes
```

## See also

- [Atomic](Atomic.md), what the pointer is; [List](../Containers/List.md) and the other containers as values
- [ConcurrentSortedDictionary](ConcurrentSortedDictionary.md), [ConcurrentDictionary](ConcurrentDictionary.md) for a value changed in place by many threads
- [README: Lock-free containers](../../concurrent/README.md#lock-free-containers), [README: The rules](../../core/README.md#the-rules)

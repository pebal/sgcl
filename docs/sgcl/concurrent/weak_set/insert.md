[sgcl](../../README.md) › [concurrent](../README.md) › [weak_set](README.md)

# sgcl::concurrent::weak_set\<Key\>::insert

```cpp
pair<iterator, bool> insert(const key_pointer& object) noexcept;
```

Inserts `object`, unless the set holds it. The table is searched once: when the object has an entry, nothing is
built, not even the weak pointer; when it has none, the node is made with the object's weak pointer and the hash of
its address, and linked where the search found its place with a compare-exchange. When another thread links an entry
for the same object first, the search is repeated from there and finds it.

`object` may not be null: a null pointer is not an object, and debug builds assert.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object to insert |

## Return value

A pair of an iterator to the entry of `object`, holding the node and the object, and `true` when this call inserted
it; or the iterator to the entry already there and `false`.

## Complexity

Constant on average, plus, when the insertion brings the count since the last sweep to the threshold, a
[sweep](sweep.md), linear in the number of entries: amortized constant, as the threshold is the number of entries
the set had after the last sweep, 16 at least.

## Exceptions

None.

## Notes

Lock-free, and linearizable at the compare-exchange that links the node: of two threads inserting the same object,
exactly one gets `true`, and the other the entry the first made.

Every insertion counts towards the next sweep. The thread whose insertion brings the count since the last sweep to
the threshold runs the sweep itself, before `insert` returns, unless another thread's sweep is under way; then it
goes on without waiting, so an insertion never waits for a sweep.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Listener {
    int id;
};

int main() {
    concurrent::weak_set<Listener> listeners;
    tracked_ptr first = make_tracked<Listener>(0);
    bool added = listeners.insert(first).second;
    bool added_again = listeners.insert(first).second;
    println("{} {}", added, added_again);

    for (int i : range(1, 11)) {
        listeners.insert(make_tracked<Listener>(i));  // nobody else holds these
    }
    collector::force_collect(true);  // optional, for the demonstration: the ten die
    println("{} entries", listeners.size());

    vector<tracked_ptr<Listener>> kept;
    for (int i : range(11, 16)) {
        kept.push_back(make_tracked<Listener>(i));
        listeners.insert(kept.back());  // the sixteenth insertion sweeps
    }
    println("{} entries", listeners.size());
}
```

Output:

```text
true false
11 entries
6 entries
```

## See also

- [sweep](sweep.md): erases the dead entries on demand
- [find](find.md): the entry of an object
- [sgcl::concurrent::weak_set\<Key\>](README.md)

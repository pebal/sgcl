[sgcl](../../README.md) › [core](../README.md) › [weak_set](../weak_set.md)

# sgcl::weak_set\<Key\>::insert

```cpp
pair<iterator, bool> insert(const key_pointer& object) noexcept;
```

Inserts `object`, unless the set holds it. The table is searched once: when the object has an entry, nothing is
built, not even the weak pointer; when it has none, the entry is made from the pointer where the search found its
place.

`object` may not be null: a null pointer is not an object, and debug builds assert.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object to insert |

## Return value

A pair of an iterator to the entry of `object` and `true` when this call inserted it; or the iterator to the entry
already there and `false`.

## Complexity

Constant on average, plus, when the insertion brings the count since the last sweep past the threshold, a
[sweep](sweep.md), linear in the number of entries: amortized constant, as the threshold is the number of entries
the set had after the last sweep, 16 at least.

## Exceptions

None.

## Notes

Every entry added counts towards the next sweep, which the insertion that brings the count past the threshold runs
before it returns.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Listener {
    int id;
};

void register_temporaries(weak_set<Listener>& listeners) {
    for (int i : range(1, 11)) {
        tracked_ptr listener = make_tracked<Listener>(i);
        listeners.insert(listener);
    }
}

int main() {
    weak_set<Listener> listeners;
    tracked_ptr first = make_tracked<Listener>(0);
    auto [it, added] = listeners.insert(first);
    bool added_again = listeners.insert(first).second;
    println("{} {} {}", (*it)->id, added, added_again);

    register_temporaries(listeners);
    collector::clear_stack();  // the dead frame zeroed: nothing on the stack keeps the listeners
    collector::force_collect(true);  // optional, for the demonstration
    println("{} entries", listeners.size());

    vector<tracked_ptr<Listener>> kept;
    for (int i : range(11, 17)) {
        kept.push_back(make_tracked<Listener>(i));
        listeners.insert(kept.back());  // the seventeenth insertion sweeps
    }
    println("{} entries", listeners.size());
}
```

Output:

```text
0 true false
11 entries
7 entries
```

## See also

- [sweep](sweep.md): erases the dead entries on demand
- [find](find.md): the entry of an object
- [sgcl::weak_set\<Key\>](../weak_set.md)

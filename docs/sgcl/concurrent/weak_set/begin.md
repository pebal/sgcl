[sgcl](../../README.md) › [concurrent](../README.md) › [weak_set](README.md)

# sgcl::concurrent::weak_set\<Key\>::begin

```cpp
iterator begin() noexcept;
```

Returns an iterator to the first live object, or [end](end.md) when there is none. The iterator walks the table's
list from its head, in the order of the list (the bit reversal of the hashes of the objects' addresses, an order of
no meaning to the program), and stops at the first entry whose object it can hold: it takes the object from the
entry's weak pointer as a `tracked_ptr`, and passes over the entries whose objects are gone without dropping them.
Each step of the iterator does the same, so a walk visits every live object once.

## Parameters

None.

## Return value

An iterator to the first live object; `*it` is the object as a `key_pointer`, held while the iterator stands on it.
`it->` is the pointer's own `->`, so a member of the object is `(*it)->member`.

## Complexity

Constant when a live entry stands near the head of the list; at worst linear in the number of buckets in use and of
dead entries before the first live one, which the walk passes.

## Exceptions

None.

## Notes

The walk is weakly consistent: an iterator holds its node and the object it stands on, so it is valid whatever the
other threads do and the object cannot die under it; it skips the entries erased since it passed them and may or may
not see the ones inserted meanwhile. A step never waits and writes nothing but the iterator.

`begin` is not `const` and there is no `cbegin`: standing on an entry holds its object, which is a write to the
iterator, not to the set.

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
    vector<tracked_ptr<Listener>> owned;
    for (int i : range(3)) {
        owned.push_back(make_tracked<Listener>(i));
        listeners.insert(owned.back());
    }

    vector<int> ids;
    for (auto it = listeners.begin(); it != listeners.end(); ++it) {
        ids.push_back((*it)->id);
    }
    ids.sort();  // the order of the table is the hashes'
    println("{}", ids);
}
```

Output:

```text
[0, 1, 2]
```

## See also

- [end](end.md): the iterator past the last entry
- [find](find.md): an iterator to the entry of an object
- [sgcl::concurrent::weak_set\<Key\>](README.md)

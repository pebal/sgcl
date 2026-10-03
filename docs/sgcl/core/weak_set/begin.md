[sgcl](../../README.md) › [core](../README.md) › [weak_set](README.md)

# sgcl::weak_set\<Key\>::begin, cbegin

```cpp
iterator begin() noexcept;                 // (1)
const_iterator begin() const noexcept;     // (2)
const_iterator cbegin() const noexcept;    // (3)
```

Returns an iterator to the first live object, or [end](end.md) when there is none. The walk visits the live objects
in no particular order, the table's, each once; it takes each object from its entry's weak pointer as a
`tracked_ptr`, and passes over the entries whose objects are gone without dropping them. `iterator` and
`const_iterator` are one type: the iterator gives out the object, and nothing of the set to write.

## Parameters

None.

## Return value

An iterator to the first live object, or `end()`; `*it` is the object as a `key_pointer`, held, and `it->` the
pointer's own `->`, so `(*it)->member`.

## Complexity

Constant, plus the dead entries before the first live one, which the walk passes.

## Exceptions

None.

## Notes

The iterator holds the object it stands on as a strong pointer, so the object cannot die under it; an iterator is a
tracked object then, and lives where the set's pointers may. Standing on an object is a write to the iterator, not
to the set, so a `const` set is walked as a mutable one is.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Listener {
    int id;
    int calls = 0;
};

int main() {
    weak_set<Listener> listeners;
    vector<tracked_ptr<Listener>> owned;
    for (int i : range(1, 4)) {
        owned.push_back(make_tracked<Listener>(i));
        listeners.insert(owned.back());
    }

    for (tracked_ptr listener : listeners) {
        listener->calls += listener->id;
    }
    int sum = 0;
    for (auto it = listeners.cbegin(); it != listeners.cend(); ++it) {
        sum += (*it)->calls;
    }
    println("{}", sum);
}
```

Output:

```text
6
```

## See also

- [end, cend](end.md): the iterator past the last entry
- [find](find.md): an iterator to the entry of an object
- [sgcl::weak_set\<Key\>](README.md)

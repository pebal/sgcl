[sgcl](../../README.md) › [core](../README.md) › [weak_set](../weak_set.md)

# sgcl::weak_set\<Key\>::operator=

```cpp
/*(1)*/ weak_set& operator=(weak_set&& other) noexcept;
/*(2)*/ weak_set& operator=(const weak_set&) = delete;
```

Replaces the entries of the set.

1. Erases the entries of this set and takes the table of `other` over, no entry copied or moved; `other` is empty
   after. Assigning a set to itself changes nothing.
2. The set is not copyable, as its [constructor](weak_set.md) says.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the set whose entries are taken over |

## Return value

`*this`.

## Complexity

Linear in the size of this set, whose entries are erased; constant in the size of `other`.

## Exceptions

None.

## Notes

Every iterator to this set is invalid after the assignment.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Listener {
    int id;
};

int main() {
    tracked_ptr a = make_tracked<Listener>(1);
    tracked_ptr b = make_tracked<Listener>(2);
    weak_set<Listener> listeners;
    listeners.insert(a);
    weak_set<Listener> other;
    other.insert(b);

    listeners = std::move(other);
    println("{} {} {}", listeners.contains(a), listeners.contains(b), other.empty());
}
```

Output:

```text
false true true
```

## See also

- [(constructor)](weak_set.md): constructs the set
- [clear](clear.md): erases every entry
- [sgcl::weak_set\<Key\>](../weak_set.md)

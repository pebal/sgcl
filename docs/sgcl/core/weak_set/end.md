[sgcl](../../README.md) › [core](../README.md) › [weak_set](README.md)

# sgcl::weak_set\<Key\>::end, cend

```cpp
iterator end() noexcept;                 // (1)
const_iterator end() const noexcept;     // (2)
const_iterator cend() const noexcept;    // (3)
```

Returns the iterator past the last entry. A walk from [begin](begin.md) reaches it after the last live object, and
[find](find.md) returns it for an object the set does not hold.

## Parameters

None.

## Return value

The iterator past the last entry. It may not be dereferenced.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Listener {
    int id;
};

int main() {
    weak_set<Listener> listeners;
    println("{}", listeners.begin() == listeners.end());

    tracked_ptr listener = make_tracked<Listener>(1);
    listeners.insert(listener);
    println("{}", listeners.cbegin() == listeners.cend());

    tracked_ptr other = make_tracked<Listener>(2);
    println("{}", listeners.find(other) == listeners.end());
}
```

Output:

```text
true
false
true
```

## See also

- [begin, cbegin](begin.md): an iterator to the first live object
- [sgcl::weak_set\<Key\>](README.md)

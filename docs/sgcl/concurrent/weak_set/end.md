[sgcl](../../README.md) › [concurrent](../README.md) › [weak_set](README.md)

# sgcl::concurrent::weak_set\<Key\>::end

```cpp
iterator end() noexcept;
```

Returns the iterator past the last entry: the end of the table's list, which holds nothing. A walk from
[begin](begin.md) reaches it after the last live object, and [find](find.md) returns it for an object the set does
not hold.

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
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Listener {
    int id;
};

int main() {
    concurrent::weak_set<Listener> listeners;
    println("{}", listeners.begin() == listeners.end());

    tracked_ptr a = make_tracked<Listener>(1);
    listeners.insert(a);
    println("{}", listeners.begin() == listeners.end());

    tracked_ptr b = make_tracked<Listener>(2);
    println("{}", listeners.find(b) == listeners.end());
}
```

Output:

```text
true
false
true
```

## See also

- [begin](begin.md): an iterator to the first live object
- [sgcl::concurrent::weak_set\<Key\>](README.md)

[sgcl](../../README.md) › [core](../README.md) › [weak_set](README.md)

# sgcl::weak_set\<Key\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the set holds an entry. An entry whose object is gone and that no sweep has dropped yet is an entry:
`empty` answers for the table, as [size](size.md) counts it, not for the live objects.

## Parameters

None.

## Return value

`true` when the set holds no entry, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Notes

The answer is exact for the live objects right after a [sweep](sweep.md). Whether the set holds a live object is
what [begin](begin.md) answers: `begin() == end()`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Listener {
    int id;
};

void register_a_temporary(weak_set<Listener>& listeners) {
    tracked_ptr listener = make_tracked<Listener>(1);
    listeners.insert(listener);
}

int main() {
    weak_set<Listener> listeners;
    println("{}", listeners.empty());

    register_a_temporary(listeners);
    collector::clear_stack();  // the dead frame zeroed: nothing on the stack keeps the listener
    collector::force_collect(true);  // optional, for the demonstration
    println("{} {}", listeners.empty(), listeners.begin() == listeners.end());

    listeners.sweep();
    println("{}", listeners.empty());
}
```

Output:

```text
true
false true
true
```

## See also

- [size](size.md): the number of entries
- [sweep](sweep.md): erases the dead entries
- [sgcl::weak_set\<Key\>](README.md)

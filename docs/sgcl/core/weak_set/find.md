[sgcl](../../README.md) › [core](../README.md) › [weak_set](../weak_set.md)

# sgcl::weak_set\<Key\>::find

```cpp
iterator find(const key_pointer& object) noexcept;                // (1)
const_iterator find(const key_pointer& object) const noexcept;    // (2)
```

Finds the entry of `object`: a search of the table by the object's address. A null pointer has no entry. A dead
entry equals nothing, so it is never found, and an object that later takes the same address is not in the set
until it is inserted.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object to find |

## Return value

An iterator to the entry of `object`, holding the object, or [end](end.md) when the set does not hold `object` or
it is null.

## Complexity

Constant on average.

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
    tracked_ptr a = make_tracked<Listener>(1);
    tracked_ptr b = make_tracked<Listener>(2);
    listeners.insert(a);

    auto it = listeners.find(a);
    if (it != listeners.end()) {
        println("{} {}", (*it)->id, *it == a);
    }
    println("{}", listeners.find(b) == listeners.end());
    println("{}", listeners.find(nullptr) == listeners.end());
}
```

Output:

```text
1 true
true
true
```

## See also

- [contains](contains.md): checks whether the set holds an object
- [count](count.md): the number of entries of an object
- [sgcl::weak_set\<Key\>](../weak_set.md)

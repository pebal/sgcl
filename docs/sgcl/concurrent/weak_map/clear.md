[sgcl](../../README.md) › [concurrent](../README.md) › [weak_map](README.md)

# sgcl::concurrent::weak_map\<Key, T\>::clear

```cpp
void clear() noexcept;
```

Erases every entry there is at the time of the walk, dead or alive: a walk of the table's list that erases each
entry it reaches, as [erase](erase.md) does. The count of insertions towards the next sweep starts again from zero,
with the threshold back at 16.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of entries.

## Exceptions

None.

## Notes

`clear` is a walk of erasures, not one exchange: under concurrent insertions an entry inserted while it runs may be
erased or left, and the map may hold entries when it returns. Each erasure is lock-free. The values are not
destroyed by the call: the collector destroys each with its node, once nothing holds the node.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

int main() {
    concurrent::weak_map<Node, string> names;
    tracked_ptr a = make_tracked<Node>(1);
    tracked_ptr b = make_tracked<Node>(2);
    names.try_emplace(a, "a");
    names.try_emplace(b, "b");

    names.clear();
    println("{} {} {}", names.empty(), names.size(), names.contains(a));
}
```

Output:

```text
true 0 false
```

## See also

- [erase](erase.md): erases one entry
- [sweep](sweep.md): erases the dead entries alone
- [sgcl::concurrent::weak_map\<Key, T\>](README.md)

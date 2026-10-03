[sgcl](../../README.md) › [core](../README.md) › [weak_map](../weak_map.md)

# sgcl::weak_map\<Key, T\>::clear

```cpp
void clear() noexcept;
```

Erases every entry, dead or alive, and destroys the values at once; the table keeps its buckets. The count of
insertions towards the next sweep starts again from zero, with the threshold back at 16.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of entries.

## Exceptions

None.

## Notes

Every iterator to the map is invalid after the call. The nodes are left to the collector.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

int main() {
    weak_map<Node, string> names;
    tracked_ptr a = make_tracked<Node>(1);
    tracked_ptr b = make_tracked<Node>(2);
    names[a] = "a";
    names[b] = "b";

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
- [sgcl::weak_map\<Key, T\>](../weak_map.md)

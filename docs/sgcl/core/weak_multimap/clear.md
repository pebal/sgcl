[sgcl](../../README.md) › [core](../README.md) › [weak_multimap](../weak_multimap.md)

# sgcl::weak_multimap\<Key, T\>::clear

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
    weak_multimap<Node, string> tags;
    tracked_ptr a = make_tracked<Node>(1);
    tracked_ptr b = make_tracked<Node>(2);
    tags.insert(a, "x");
    tags.insert(a, "y");
    tags.insert(b, "z");

    tags.clear();
    println("{} {} {}", tags.empty(), tags.size(), tags.contains(a));
}
```

Output:

```text
true 0 false
```

## See also

- [erase](erase.md): erases the entries of an object, or one entry
- [sweep](sweep.md): erases the dead entries alone
- [sgcl::weak_multimap\<Key, T\>](../weak_multimap.md)

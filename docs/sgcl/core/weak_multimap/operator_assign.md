[sgcl](../../README.md) › [core](../README.md) › [weak_multimap](../weak_multimap.md)

# sgcl::weak_multimap\<Key, T\>::operator=

```cpp
weak_multimap& operator=(weak_multimap&& other) noexcept;    // (1)
weak_multimap& operator=(const weak_multimap&) = delete;     // (2)
```

Replaces the entries of the map.

1. Destroys the values of this map at once and takes the table of `other` over, no entry copied or moved; `other`
   is empty after. Assigning a map to itself changes nothing.
2. The map is not copyable, as its [constructor](weak_multimap.md) says.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the map whose entries are taken over |

## Return value

`*this`.

## Complexity

Linear in the size of this map, whose values are destroyed; constant in the size of `other`.

## Exceptions

None.

## Notes

Every iterator to this map is invalid after the assignment.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

int main() {
    tracked_ptr a = make_tracked<Node>(1);
    tracked_ptr b = make_tracked<Node>(2);
    weak_multimap<Node, string> tags;
    tags.insert(a, "x");
    weak_multimap<Node, string> other;
    other.insert(b, "y");
    other.insert(b, "z");

    tags = std::move(other);
    println("{} {} {}", tags.contains(a), tags.count(b), other.empty());
}
```

Output:

```text
false 2 true
```

## See also

- [(constructor)](weak_multimap.md): constructs the map
- [clear](clear.md): erases every entry
- [sgcl::weak_multimap\<Key, T\>](../weak_multimap.md)

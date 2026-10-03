[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::clear

```cpp
void clear() noexcept;
```

Destroys every element at once and unlinks every node. The bucket array, the hasher, the equality and
`max_load_factor` stay: the map refills without growing again.

## Parameters

None.

## Return value

None.

## Complexity

Linear in `size()` and `bucket_count()`.

## Exceptions

None.

## Notes

The nodes are reclaimed by the collector, not freed here. Every iterator to an element is invalidated.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int alive = 0;

struct Counted {
    Counted() { ++alive; }
    ~Counted() { --alive; }
};

int main() {
    map<int, Counted> m;
    for (int i : range(3)) {
        m[i];  // a Counted built in place
    }
    auto buckets = m.bucket_count();
    println("{} {}", alive, m.size());

    m.clear();  // the three are destroyed here
    println("{} {} {}", alive, m.size(), m.bucket_count() == buckets);
}
```

Output:

```text
3 3
0 0 true
```

## See also

- [erase](erase.md): erases elements
- [erase_if](erase_if.md): erases every element satisfying a predicate
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)

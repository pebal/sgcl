[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::clear

```cpp
void clear() noexcept;
```

Erases every element: each is destroyed at once and its node unlinked from the chain and from the order. The
bucket array, the sentinel, the hash, the equality and `max_load_factor` stay; the nodes are reclaimed by the
collector.

## Parameters

None.

## Return value

None.

## Complexity

Linear in `size()` and in `bucket_count()`.

## Exceptions

None.

## Notes

Every iterator to an element is invalidated; [end()](end.md) is not, the sentinel being kept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<int, string> m = {{1, "a"}, {2, "b"}};
    auto buckets = m.bucket_count();
    m.clear();  // both strings are destroyed here
    println("{} {}", m.size(), m.bucket_count() == buckets);

    m[3] = "c";
    println("{}", m);
}
```

Output:

```text
0 true
{3: "c"}
```

## See also

- [erase](erase.md): erases elements
- [erase_if](erase_if.md): erases the elements a predicate accepts
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)

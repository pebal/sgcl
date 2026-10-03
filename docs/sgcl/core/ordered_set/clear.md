[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::clear

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
    ordered_set<string> s = {"a", "b"};
    auto buckets = s.bucket_count();
    s.clear();  // both strings are destroyed here
    println("{} {}", s.size(), s.bucket_count() == buckets);

    s.insert("c");
    println("{}", s);
}
```

Output:

```text
0 true
{"c"}
```

## See also

- [erase](erase.md): erases elements
- [erase_if](erase_if.md): erases the elements a predicate accepts
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](../ordered_set.md)

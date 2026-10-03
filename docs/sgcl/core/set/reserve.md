[sgcl](../../README.md) › [core](../README.md) › [set](../set.md)

# sgcl::set\<Key, Hash, KeyEqual\>::reserve

```cpp
void reserve(size_type count) noexcept;
```

Sets the number of buckets for `count` elements: [rehash](rehash.md) of `count / max_load_factor()`, rounded up.
The insertion of `count` elements then grows the table no more. As with `rehash`, the table may shrink when
`count` asks for fewer buckets than there are, though never below what the elements need.

A `count` whose buckets the managed heap cannot give, up to `SIZE_MAX`, ends the program as any refused managed
allocation does ([collector](../collector.md#the-memory-limit)), as [rehash](rehash.md) of their number does.

## Parameters

| Parameter | Description |
|---|---|
| `count` | the number of elements to make room for |

## Return value

None.

## Complexity

Linear in `size()` and the new bucket count.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    set<int> s;
    s.reserve(1000);
    println("{}", s.bucket_count());

    for (int i : range(1000)) {
        s.insert(i);
    }
    println("{} {}", s.size(), s.bucket_count());
}
```

Output:

```text
1024
1000 1024
```

## See also

- [rehash](rehash.md): sets the number of buckets
- [(constructor)](set.md): a set with its buckets from the start
- [sgcl::set\<Key, Hash, KeyEqual\>](../set.md)

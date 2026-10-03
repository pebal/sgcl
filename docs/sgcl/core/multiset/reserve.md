[sgcl](../../README.md) › [core](../README.md) › [multiset](README.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>::reserve

```cpp
void reserve(size_type count) noexcept;
```

Sets the number of buckets for `count` elements, equal ones counted each: [rehash](rehash.md) of
`count / max_load_factor()`, rounded up. The insertion of `count` elements then grows the table no more. As with
`rehash`, the table may shrink when `count` asks for fewer buckets than there are, though never below what the
elements need.

A `count` whose buckets the managed heap cannot give, up to `SIZE_MAX`, ends the program as any refused managed
allocation does ([collector](../collector/README.md#the-memory-limit)), as [rehash](rehash.md) of their number does.

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
    multiset<int> s;
    s.reserve(1000);
    println("{}", s.bucket_count());

    for (int i : range(1000)) {
        s.insert(i % 10);  // ten runs of a hundred
    }
    println("{} {} {}", s.size(), s.count(7), s.bucket_count());
}
```

Output:

```text
1024
1000 100 1024
```

## See also

- [rehash](rehash.md): sets the number of buckets
- [(constructor)](multiset.md): a multiset with its buckets from the start
- [sgcl::multiset\<Key, Hash, KeyEqual\>](README.md)

[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::reserve

```cpp
void reserve(size_type count) noexcept;
```

Sets the number of buckets for `count` elements, [rehash](rehash.md)`(count / max_load_factor())` rounded up:
the map then takes `count` elements without growing. As `rehash`, it may shrink a table larger than it needs.

A `count` whose buckets the managed heap cannot give, up to `SIZE_MAX`, ends the program as any refused managed
allocation does ([collector](../collector.md#the-memory-limit)), as [rehash](rehash.md) of their number does.

## Parameters

| Parameter | Description |
|---|---|
| `count` | the number of elements to make room for |

## Return value

None.

## Complexity

Linear in `size()` and the new `bucket_count()`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<int, int> m;
    m.reserve(1000);
    auto buckets = m.bucket_count();
    for (int i : range(1000)) {
        m.emplace(i, i);
    }
    println("{} {}", buckets, m.bucket_count() == buckets);

    m.max_load_factor(0.5f);
    m.reserve(1000);
    println("{}", m.bucket_count());
}
```

Output:

```text
1024 true
2048
```

## See also

- [rehash](rehash.md): sets the number of buckets
- [max_load_factor](max_load_factor.md): the load factor the table grows at
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)

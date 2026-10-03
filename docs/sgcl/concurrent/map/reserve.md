[sgcl](../../README.md) › [concurrent](../README.md) › [map](../map.md)

# sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>::reserve

```cpp
void reserve(size_type count) noexcept;
```

Grows the bucket array to at least `count` buckets, doubling it as the insertions would, so that `count` elements
fit without a growth on the way: the array is grown now rather than by the insertions. An array that has `count`
buckets already is left as it is; the array never shrinks.

## Parameters

| Parameter | Description |
|---|---|
| `count` | the number of elements to make room for |

## Return value

None.

## Complexity

Linear in the number of buckets after the call: each doubling copies the slots of the array.

## Exceptions

None.

## Notes

May run concurrently with anything. One thread doubles the array at a time: while another thread doubles it,
`reserve` tries again until the array has the buckets asked for, so it may spin for the length of another
thread's doubling. The buckets of the new half get their dummy nodes on their first use, not here.

A `count` whose buckets the managed heap cannot give, up to `SIZE_MAX`, ends the program at the doubling it refuses,
as any refused managed allocation does ([collector](../../core/collector.md#the-memory-limit)).

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::map<int, int> ids;
    ids.reserve(100000);
    println("{}", ids.bucket_count());

    for (int i : range(100000)) {
        ids.try_emplace(i, i);
    }
    println("{} {}", ids.size(), ids.bucket_count());

    ids.reserve(10);
    println("{}", ids.bucket_count());
}
```

Output:

```text
131072
100000 131072
131072
```

## See also

- [bucket_count](bucket_count.md): the number of buckets, and when the array doubles
- [(constructor)](map.md): a map with its buckets from the start
- [sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>](../map.md)

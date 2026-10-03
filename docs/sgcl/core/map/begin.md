[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::begin, cbegin

```cpp
iterator begin() noexcept;                                  // (1)
const_iterator begin() const noexcept;                      // (2)
const_iterator cbegin() const noexcept;                     // (3)
local_iterator begin(size_type n) noexcept;                 // (4)
const_local_iterator begin(size_type n) const noexcept;     // (5)
const_local_iterator cbegin(size_type n) const noexcept;    // (6)
```

Returns an iterator to the first element.

- (1–3) The first element of the map, in the order of the chain of nodes: the elements of a bucket together,
  the buckets in no particular order. The order changes with a rehash and stays otherwise. An empty map gives
  [end()](end.md).
- (4–6) The first element of bucket `n`. A local iterator walks the nodes of that bucket and stops at its end,
  [end(n)](end.md); for an `n` that is not below `bucket_count()`, the range is empty.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of a bucket |

## Return value

- (1–3) An iterator to the first element, or `end()` when the map is empty.
- (4–6) A local iterator to the first element of bucket `n`, or `end(n)` when the bucket is empty.

## Complexity

Constant.

## Exceptions

None.

## Notes

An iterator is a raw node pointer: copying and advancing it costs a load, never a write barrier, and it may be
kept in unmanaged memory (a `std::vector` of iterators) while its element is in the map, across rehashes. A local
iterator also holds the bucket and the mask of the table: a rehash, by `rehash`, `reserve` or an insertion that
grows the table, invalidates it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <vector>

using namespace sgcl;

int main() {
    map<string, int> m = {{"a", 1}, {"b", 2}, {"c", 3}};
    int sum = 0;
    for (auto it = m.cbegin(); it != m.cend(); ++it) {
        sum += it->second;
    }
    println("{}", sum);

    std::vector<map<string, int>::iterator> kept;  // iterators in unmanaged memory
    kept.push_back(m.begin());
    m.rehash(64);
    println("{}", kept[0] == m.find(kept[0]->first));

    size_t n = m.bucket("a");
    size_t in_bucket = 0;
    for (auto it = m.begin(n); it != m.end(n); ++it) {
        ++in_bucket;
    }
    println("{} {}", in_bucket == m.bucket_size(n), m.begin(64) == m.end(64));
}
```

Output:

```text
6
true
true true
```

## See also

- [end, cend](end.md): the iterator past the last element
- [find](find.md): an iterator to the element under a key
- [bucket](bucket.md): the bucket of a key
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)

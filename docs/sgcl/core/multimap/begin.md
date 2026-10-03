[sgcl](../../README.md) › [core](../README.md) › [multimap](../multimap.md)

# sgcl::multimap\<Key, T, Hash, KeyEqual\>::begin, cbegin

```cpp
iterator begin() noexcept;                                  // (1)
const_iterator begin() const noexcept;                      // (2)
const_iterator cbegin() const noexcept;                     // (3)
local_iterator begin(size_type n) noexcept;                 // (4)
const_local_iterator begin(size_type n) const noexcept;     // (5)
const_local_iterator cbegin(size_type n) const noexcept;    // (6)
```

Returns an iterator to the first element.

- (1–3) The first element of the multimap, in the order of the chain of nodes: the elements of a bucket together,
  the elements of one key adjacent among them, the buckets in no particular order. The order changes with a
  rehash, which keeps each run of equal keys together and in its order. An empty multimap gives [end()](end.md).
- (4–6) The first element of bucket `n`. A local iterator walks the nodes of that bucket, equivalent keys
  adjacent, and stops at its end, [end(n)](end.md); for an `n` that is not below `bucket_count()`, the range is
  empty.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of a bucket |

## Return value

- (1–3) An iterator to the first element, or `end()` when the multimap is empty.
- (4–6) A local iterator to the first element of bucket `n`, or `end(n)` when the bucket is empty.

## Complexity

Constant.

## Exceptions

None.

## Notes

An iterator is a raw node pointer: copying and advancing it costs a load, never a write barrier, and it may be
kept in unmanaged memory (a `std::vector` of iterators) while its element is in the multimap, across rehashes. A
local iterator also holds the bucket and the mask of the table: a rehash, by `rehash`, `reserve` or an insertion
that grows the table, invalidates it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multimap<string, int> m = {{"a", 1}, {"b", 3}, {"a", 2}};
    int total = 0;
    string previous;
    size_t runs = 0;
    for (auto it = m.cbegin(); it != m.cend(); ++it) {
        total += it->second;
        if (it->first != previous) {
            ++runs;  // a new key: the elements of a key are adjacent
            previous = it->first;
        }
    }
    println("{} {}", total, runs);

    size_t n = m.bucket("a");
    size_t in_bucket = 0;
    for (auto it = m.begin(n); it != m.end(n); ++it) {
        ++in_bucket;
    }
    println("{} {}", in_bucket == m.bucket_size(n), in_bucket >= 2);
}
```

Output:

```text
6 2
true true
```

## See also

- [end, cend](end.md): the iterator past the last element
- [equal_range](equal_range.md): the run of the elements under a key
- [bucket](bucket.md): the bucket of a key
- [sgcl::multimap\<Key, T, Hash, KeyEqual\>](../multimap.md)

[sgcl](../../README.md) › [core](../README.md) › [multiset](../multiset.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>::begin, cbegin

```cpp
iterator begin() noexcept;                                  // (1)
const_iterator begin() const noexcept;                      // (2)
const_iterator cbegin() const noexcept;                     // (3)
local_iterator begin(size_type n) noexcept;                 // (4)
const_local_iterator begin(size_type n) const noexcept;     // (5)
const_local_iterator cbegin(size_type n) const noexcept;    // (6)
```

Returns an iterator to the first element.

- (1–3) The first element of the multiset, in the order of the chain of nodes: the elements of a bucket
  together, equal elements adjacent, the buckets in no particular order. The order changes with a rehash and
  stays otherwise, the runs of equal elements together and in their order. An empty multiset gives
  [end()](end.md).
- (4–6) The first element of bucket `n`. A local iterator walks the nodes of that bucket, equal elements
  adjacent, and stops at its end, [end(n)](end.md); for an `n` that is not below `bucket_count()`, the range is
  empty.

`iterator` and `const_iterator` are one type, and so are the local ones: they yield `const Key&`.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of a bucket |

## Return value

- (1–3) An iterator to the first element, or `end()` when the multiset is empty.
- (4–6) A local iterator to the first element of bucket `n`, or `end(n)` when the bucket is empty.

## Complexity

Constant.

## Exceptions

None.

## Notes

An iterator is a raw node pointer: copying and advancing it costs a load, and it may be kept in unmanaged
memory (a `std::vector` of iterators) while its element is in the multiset, across rehashes. A local iterator
also holds the bucket and the mask of the table: a rehash invalidates it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multiset<string> s = {"b", "a", "b", "c", "b"};
    vector<string> keys(s.begin(), s.end());
    keys.sort();
    println("{}", keys);

    size_t run = 0;
    for (auto it = s.find("b"); it != s.end() && *it == "b"; ++it) {
        ++run;  // equal elements are adjacent
    }
    println("{}", run);

    size_t n = s.bucket("b");
    size_t in_bucket = 0;
    for (auto it = s.cbegin(n); it != s.cend(n); ++it) {
        ++in_bucket;
    }
    println("{} {}", in_bucket >= 3, in_bucket == s.bucket_size(n));
}
```

Output:

```text
["a", "b", "b", "b", "c"]
3
true true
```

## See also

- [end, cend](end.md): the iterator past the last element
- [equal_range](equal_range.md): the run of the elements with a key
- [bucket](bucket.md): the bucket of a key
- [sgcl::multiset\<Key, Hash, KeyEqual\>](../multiset.md)

[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::begin, cbegin

```cpp
iterator begin() noexcept;                                  // (1)
const_iterator begin() const noexcept;                      // (2)
const_iterator cbegin() const noexcept;                     // (3)
local_iterator begin(size_type n) noexcept;                 // (4)
const_local_iterator begin(size_type n) const noexcept;     // (5)
const_local_iterator cbegin(size_type n) const noexcept;    // (6)
```

- (1–3) Returns an iterator to the oldest element, the first of the order of insertion. A walk from it to
  [end()](end.md) visits the elements oldest first, whatever the hashes, and a rehash does not change it. An
  empty map gives `end()`.
- (4–6) Returns a local iterator to the first element of bucket `n`: a walk of the bucket's part of the chain,
  in the chain's order, not in the order of insertion, which stops at the end of the bucket. An `n` not below
  [bucket_count()](bucket_count.md) gives an empty range.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of the bucket |

## Return value

- (1–3) An iterator to the first element of the order, or `end()` when the map is empty.
- (4–6) A local iterator to the first element of bucket `n`, or `end(n)` when the bucket is empty.

## Complexity

Constant.

## Exceptions

None.

## Notes

An iterator is one raw node pointer, bidirectional: copying it, advancing it and stepping it back each cost a
load, never a write barrier, and it may be kept in memory of any kind, a `std::vector` included, while its
element is in the map. A local iterator holds the bucket's number and the mask besides, and only goes forward.

The members of [mixin::enumerable](../mixin/enumerable.md) walk from `begin()`: a position they give, as
`index_of`'s, is a position in the order of insertion.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<string, int> m;
    m["c"] = 1;
    m["a"] = 2;
    m["b"] = 3;
    m["a"] = 4;  // present: the value changes, the place does not

    vector<string> keys;
    for (auto it = m.cbegin(); it != m.cend(); ++it) {
        keys.push_back(it->first);
    }
    println("{} {}", keys, m.begin()->second);
    println("{}", m.index_of(pair<const string, int>("a", 4)));  // a position in the order

    size_t n = m.bucket("a");
    size_t in_bucket = 0;
    for (auto it = m.begin(n); it != m.end(n); ++it) {
        ++in_bucket;
    }
    println("{}", in_bucket == m.bucket_size(n));
}
```

Output:

```text
["c", "a", "b"] 1
1
true
```

## See also

- [end, cend](end.md): the iterator past the newest element
- [rbegin, crbegin](rbegin.md): the order from the newest element back
- [bucket](bucket.md), [bucket_size](bucket_size.md): the bucket of a key, the elements in a bucket
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)

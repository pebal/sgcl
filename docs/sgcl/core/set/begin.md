[sgcl](../../README.md) › [core](../README.md) › [set](README.md)

# sgcl::set\<Key, Hash, KeyEqual\>::begin, cbegin

```cpp
iterator begin() noexcept;                                  // (1)
const_iterator begin() const noexcept;                      // (2)
const_iterator cbegin() const noexcept;                     // (3)
local_iterator begin(size_type n) noexcept;                 // (4)
const_local_iterator begin(size_type n) const noexcept;     // (5)
const_local_iterator cbegin(size_type n) const noexcept;    // (6)
```

Returns an iterator to the first element.

- (1–3) The first element of the set, in the order of the chain of nodes: the elements of a bucket together,
  the buckets in no particular order. The order changes with a rehash and stays otherwise. An empty set gives
  [end()](end.md).
- (4–6) The first element of bucket `n`. A local iterator walks the nodes of that bucket and stops at its end,
  [end(n)](end.md); for an `n` that is not below `bucket_count()`, the range is empty.

`iterator` and `const_iterator` are one type, and so are the local ones: they yield `const Key&`.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of a bucket |

## Return value

- (1–3) An iterator to the first element, or `end()` when the set is empty.
- (4–6) A local iterator to the first element of bucket `n`, or `end(n)` when the bucket is empty.

## Complexity

Constant.

## Exceptions

None.

## Notes

An iterator is a raw node pointer: copying and advancing it costs a load, and it may be kept in unmanaged
memory (a `std::vector` of iterators) while its element is in the set, across rehashes. A local iterator also
holds the bucket and the mask of the table: a rehash invalidates it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <vector>

using namespace sgcl;

int main() {
    set<string> s = {"a", "b", "c"};
    vector<string> keys(s.begin(), s.end());
    keys.sort();
    println("{}", keys);

    std::vector<set<string>::iterator> kept;  // iterators in unmanaged memory
    kept.push_back(s.cbegin());
    s.rehash(64);
    println("{}", kept[0] == s.find(*kept[0]));

    size_t n = s.bucket("a");
    size_t in_bucket = 0;
    for (auto it = s.begin(n); it != s.end(n); ++it) {
        ++in_bucket;
    }
    println("{} {}", in_bucket == s.bucket_size(n), s.begin(64) == s.end(64));
}
```

Output:

```text
["a", "b", "c"]
true
true true
```

## See also

- [end, cend](end.md): the iterator past the last element
- [find](find.md): an iterator to the element with a key
- [bucket](bucket.md): the bucket of a key
- [sgcl::set\<Key, Hash, KeyEqual\>](README.md)

[sgcl](../../README.md) › [core](../README.md) › [multimap](README.md)

# sgcl::multimap\<Key, T, Hash, KeyEqual\>::end, cend

```cpp
iterator end() noexcept;                                  // (1)
const_iterator end() const noexcept;                      // (2)
const_iterator cend() const noexcept;                     // (3)
local_iterator end(size_type n) noexcept;                 // (4)
const_local_iterator end(size_type n) const noexcept;     // (5)
const_local_iterator cend(size_type n) const noexcept;    // (6)
```

Returns an iterator past the last element.

- (1–3) The end of the multimap: a null iterator, which the last element's increment reaches and which a lookup
  that finds nothing returns. It is never invalidated.
- (4–6) The end of bucket `n`, which an increment from the last element of the bucket reaches.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of a bucket |

## Return value

- (1–3) The iterator past the last element.
- (4–6) The local iterator past the last element of bucket `n`.

## Complexity

Constant.

## Exceptions

None.

## Notes

The end iterator holds no node, so the end of one multimap compares equal to the end of another; an iterator is
compared only with the end of its own multimap. A local iterator (4–6) holds the bucket and the mask of the table:
a rehash, by `rehash`, `reserve` or an insertion that grows the table, invalidates it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multimap<int, string> m = {{1, "a"}, {2, "b"}, {2, "c"}, {3, "d"}};
    for (auto it = m.begin(); it != m.end();) {
        it = it->first == 2 ? m.erase(it) : std::next(it);
    }
    println("{} {}", m.size(), m.find(2) == m.cend());

    size_t elements = 0;
    for (size_t n : range(m.bucket_count())) {
        for (auto it = m.cbegin(n); it != m.cend(n); ++it) {
            ++elements;
        }
    }
    println("{}", elements);
}
```

Output:

```text
2 true
2
```

## See also

- [begin, cbegin](begin.md): the iterator to the first element
- [erase](erase.md): erases the element at an iterator and returns the next one
- [sgcl::multimap\<Key, T, Hash, KeyEqual\>](README.md)

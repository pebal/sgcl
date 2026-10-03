[sgcl](../../README.md) › [core](../README.md) › [map](README.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::end, cend

```cpp
iterator end() noexcept;                                  // (1)
const_iterator end() const noexcept;                      // (2)
const_iterator cend() const noexcept;                     // (3)
local_iterator end(size_type n) noexcept;                 // (4)
const_local_iterator end(size_type n) const noexcept;     // (5)
const_local_iterator cend(size_type n) const noexcept;    // (6)
```

Returns an iterator past the last element.

- (1–3) The end of the map: a null iterator, which the last element's increment reaches and which a lookup that
  finds nothing returns. It is never invalidated.
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

The end iterator holds no node, so the end of one map compares equal to the end of another; an iterator is
compared only with the end of its own map. A local iterator (4–6) holds the bucket and the mask of the table:
a rehash, by `rehash`, `reserve` or an insertion that grows the table, invalidates it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<int, string> m;
    auto stop = m.end();  // kept from the empty map
    for (int i : range(1, 7)) {
        m.emplace(i, to_string(i));
    }
    println("{} {}", m.find(4) != stop, m.find(7) == stop);

    for (auto it = m.begin(); it != m.end();) {
        it = it->first % 2 ? m.erase(it) : std::next(it);
    }
    size_t elements = 0;
    for (size_t n : range(m.bucket_count())) {
        for (auto it = m.cbegin(n); it != m.cend(n); ++it) {
            ++elements;
        }
    }
    println("{} {}", m.size(), elements);
}
```

Output:

```text
true true
3 3
```

## See also

- [begin, cbegin](begin.md): the iterator to the first element
- [erase](erase.md): erases the element at an iterator and returns the next one
- [sgcl::map\<Key, T, Hash, KeyEqual\>](README.md)

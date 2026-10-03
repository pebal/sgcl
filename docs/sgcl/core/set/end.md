[sgcl](../../README.md) › [core](../README.md) › [set](README.md)

# sgcl::set\<Key, Hash, KeyEqual\>::end, cend

```cpp
iterator end() noexcept;                                  // (1)
const_iterator end() const noexcept;                      // (2)
const_iterator cend() const noexcept;                     // (3)
local_iterator end(size_type n) noexcept;                 // (4)
const_local_iterator end(size_type n) const noexcept;     // (5)
const_local_iterator cend(size_type n) const noexcept;    // (6)
```

Returns an iterator past the last element.

- (1–3) The end of the set: a null iterator, which the last element's increment reaches and which a lookup that
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

The end iterator holds no node, so the end of one set compares equal to the end of another; an iterator is
compared only with the end of its own set.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    set s = {1, 2, 3, 4, 5, 6};
    for (auto it = s.begin(); it != s.end();) {
        it = *it % 2 ? s.erase(it) : std::next(it);
    }
    println("{} {}", s.size(), s.find(1) == s.cend());

    size_t elements = 0;
    for (size_t n : range(s.bucket_count())) {
        for (auto it = s.cbegin(n); it != s.cend(n); ++it) {
            ++elements;
        }
    }
    println("{}", elements);
}
```

Output:

```text
3 true
3
```

## See also

- [begin, cbegin](begin.md): the iterator to the first element
- [erase](erase.md): erases the element at an iterator and returns the next one
- [sgcl::set\<Key, Hash, KeyEqual\>](README.md)

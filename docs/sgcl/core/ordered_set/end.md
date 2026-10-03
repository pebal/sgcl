[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::end, cend

```cpp
iterator end() noexcept;                                  // (1)
const_iterator end() const noexcept;                      // (2)
const_iterator cend() const noexcept;                     // (3)
local_iterator end(size_type n) noexcept;                 // (4)
const_local_iterator end(size_type n) const noexcept;     // (5)
const_local_iterator cend(size_type n) const noexcept;    // (6)
```

- (1–3) Returns the iterator past the newest element: the sentinel of the order, so `--end()` is the newest
  element. It is not dereferenced.
- (4–6) Returns the local iterator past the last element of bucket `n`.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of the bucket |

## Return value

- (1–3) The iterator past the last element of the order.
- (4–6) The local iterator past the last element of bucket `n`.

## Complexity

Constant.

## Exceptions

None.

## Notes

The sentinel is made with the bucket array, at the first insertion of a set constructed empty (or by
[reserve](reserve.md), [rehash](rehash.md), the constructor with a bucket count). Before that `end()` is null:
it equals `begin()` but is not decremented, and it is not the `end()` of the set after its first insertion.
From then on `end()` stays the same iterator through insertions, erasures, a [clear](clear.md) and a rehash; a
[swap](swap.md) or a move takes the sentinel, and the `end()` taken from it, to the other set.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_set<int> s = {1, 2, 3};
    println("{} {}", *std::prev(s.end()), s.find(9) == s.cend());

    auto stop = s.end();
    s.erase(1);
    s.insert(4);
    s.clear();
    println("{}", stop == s.end());

    ordered_set<int> fresh;
    auto before = fresh.end();
    fresh.insert(7);
    println("{} {}", before == fresh.end(), *std::prev(fresh.end()));
}
```

Output:

```text
3 true
true
false 7
```

## See also

- [begin, cbegin](begin.md): an iterator to the oldest element
- [rend, crend](rend.md): the reverse iterator past the oldest element
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](../ordered_set.md)

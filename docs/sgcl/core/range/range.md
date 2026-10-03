[sgcl](../../README.md) › [core](../README.md) › [range](README.md)

# sgcl::range\<It\>::range

```cpp
range() = default;                                          // (1)
range(It first, It last) noexcept;                          // (2)
template<class Pair>
requires requires(Pair p) { It(p.first); It(p.second); }
range(Pair p) noexcept;                                     // (3)
template<std::integral T>
explicit range(T last) noexcept;                            // (4)
template<std::integral T>
range(T first, T last) noexcept;                            // (5)
```

Constructs a range.

1. An empty range of two value-initialized iterators.
2. The elements `[first, last)`.
3. The elements `[p.first, p.second)`: what `equal_range` hands back. Takes part only when `It` is constructible
   from both members.
4. The integers `0, 1, ..., last - 1`; empty for a `last` of zero or below.
5. The integers `first, ..., last - 1`; empty for `first >= last`.

- (4–5) Take part only when `It` is [counting_iterator\<T\>](../counting_iterator.md), which the deduction guides
  give: `range(10)`, `range(2, 10)`. Both ends are of the one type `T`.

## Parameters

| Parameter | Description |
|---|---|
| `first`, `last` | the iterators, or the integers, at the ends |
| `p` | a pair of iterators |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <vector>

using namespace sgcl;

int main() {
    std::vector<int> v = {1, 2, 3, 4};
    range middle(v.begin() + 1, v.end() - 1);

    multimap<string, int> m = {{"a", 1}, {"a", 2}, {"b", 3}};
    range as = m.equal_range("a");

    println("{} {} {} {}", middle, as.size(), range(4), range(2, 5));
    println("{} {}", range(-1).empty(), range(5, 2).empty());
}
```

Output:

```text
[2, 3] 2 [0, 1, 2, 3] [2, 3, 4]
true true
```

## See also

- [begin](begin.md), [end](end.md): the iterators
- [sgcl::range\<It\>](README.md)

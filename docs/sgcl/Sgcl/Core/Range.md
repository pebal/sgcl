# Sgcl::Range

```cpp
#include "sgcl/Sgcl/Core/Range.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class It>
    class Range;

    template<std::integral T> Range(T last) -> Range<sgcl::detail::counter<T>>;       // 0..last
    template<std::integral T> Range(T first, T last) -> Range<sgcl::detail::counter<T>>;   // first..last
    template<class Pair> Range(Pair p) -> Range<decltype(p.first)>;                 // from an equal_range pair
}
```

The same class in the `sgcl` interface: [range](../../core/range.md).

A pair of iterators as a range, what a lookup of a key with several values hands back (`MultiDictionary::Values`, `HashMultiSet::Values`, `EqualRange` of the sorted containers): for a range-for over the run. The same class counts integers: `Range(10)` is `0, 1, ..., 9`, `Range(2, 10)` is `2, ..., 9`, the shape of Go's `for i := range 10` and C#'s `Enumerable.Range`, over an iterator that holds the number (`sgcl::detail::counter<T>`: random access to `std::ranges` through its `iterator_concept`, so `Count()` is a subtraction; to the pre-C++20 `iterator_category` an input iterator, as `std::ranges::iota_view`'s, since `*i` is a value, which the old forward category forbids). A `Range` holds two iterators and nothing else: it owns no elements, lives anywhere (it is not a managed type), and is a borrowed range to `std::ranges`, so an algorithm may hand back an iterator into a temporary. The `sgcl::range` inside is `Inner()`.

There is no step and no walk downwards: `Range(first, last)` with `first > last` is empty, not a descent, and a plain `for` says a step better than a third argument would.

## Rules

- A `Range` lives anywhere: it holds iterators, never a `Ptr` of its own. The elements it walks are the container's, and the container's rules on iterators apply: an entry removed under an iterator invalidates it, as in `std`.
- The counting forms take one integer type for both ends (`Range(size_t(4), size_t(6))`, not `Range(4, size_t(6))`); a negative `last` in `Range(last)` is empty.
- `First()` on an empty range is undefined, as `*Begin()` is.

## Members

```cpp
using Iterator = It;
using InnerType = sgcl::range<It>;
using ValueType = ...;                            // what *Begin() yields, without the reference

Range();                                          // empty
Range(It first, It last) noexcept;
template<class Pair> Range(Pair p) noexcept;      // from an equal_range pair (It from p.first)
template<std::integral T> explicit Range(T last) noexcept;   // 0..last, only for It = sgcl::detail::counter<T>
template<std::integral T> Range(T first, T last) noexcept;   // first..last, empty when first > last
explicit Range(InnerType r) noexcept;             // over an sgcl::range

bool IsEmpty() const noexcept;
size_t Count() const;                             // a subtraction for a random-access iterator, a walk for a forward one
decltype(auto) First() const noexcept;            // *Begin(); the range must not be empty
It Begin() const noexcept;
It End() const noexcept;
InnerType& Inner() noexcept;  const InnerType& Inner() const noexcept;

// free
It begin(const Range&) noexcept;
It end(const Range&) noexcept;
```

`std::ranges::enable_borrowed_range<Sgcl::Range<It>>` is `true`.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <cassert>

int main() {
    int sum = 0;
    for (int i : Range(10)) {                // 0..9
        sum += i;
    }
    assert(sum == 45);
    List<int> seen;
    for (int i : Range(2, 5)) {              // 2, 3, 4
        seen.Add(i);
    }
    assert((seen == List<int>{2, 3, 4}) && Range(7, 3).IsEmpty() && Range(3, 8).Count() == 5);

    MultiDictionary<String, int> scores;
    scores.Add("ann", 90);
    scores.Add("ann", 95);
    Range ann = scores.Values("ann");        // Range<Iterator>, deduced
    assert(!ann.IsEmpty() && ann.Count() == 2);
    int best = 0;
    for (auto& [name, score] : ann) {
        best = std::max(best, score);
    }
    assert(best == 95 && scores.Values("bob").IsEmpty());
    return 0;
}
```

## See also

- [MultiDictionary](../Containers/MultiDictionary.md), [HashMultiSet](../Containers/HashMultiSet.md), [SortedDictionary](../Containers/SortedDictionary.md): what hands a `Range` back
- [List](../Containers/List.md), [MSequence](../Containers/MSequence.md): the containers and the algorithms a counted loop indexes into

# sgcl::range

```cpp
#include "sgcl/core/range.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class It>
    class range;

    template<std::integral T> range(T last) -> range<detail::counter<T>>;       // 0..last
    template<std::integral T> range(T first, T last) -> range<detail::counter<T>>;   // first..last
    template<class Pair> range(Pair p) -> range<decltype(p.first)>;           // from equal_range
}
```

A pair of iterators as a range, half-open, for a range-for and for `std::ranges`: what `equal_range` of a [sorted_multimap](../containers/sorted_multimap.md), an [multimap](../containers/multimap.md) or a [weak_map](../containers/weak_map.md) hands back as a `std::pair`, made iterable (`sgcl::range ones = m.equal_range(1);`). The same class counts integers: `range(10)` is `0, 1, ..., 9`, `range(2, 10)` is `2, ..., 9`, the shape of Go's `for i := range 10` and C#'s `Enumerable.Range`, over an iterator that holds the number (`detail::counter<T>`: random access to `std::ranges` through its `iterator_concept`, so `size()` is a subtraction; to the pre-C++20 `iterator_category` an input iterator, as `std::ranges::iota_view`'s, since `*i` is a value, which the old forward category forbids). A `range` holds two iterators and nothing else: it owns no elements, lives anywhere (it is not a managed type), and is a borrowed range to `std::ranges`, so an algorithm may hand back an iterator into a temporary (`*std::ranges::max_element(sgcl::range(3, 8))`).

There is no step and no walk downwards: `range(first, last)` with `first > last` is empty, not a descent, and a plain `for` says a step better than a third argument would. `std::views::iota` is the standard's counting range; `sgcl::range` exists so that one class serves both the lookups and the counting, under one name; it lives in `core`, next to the aliases, because it needs nothing from the containers and every module's examples count with it.

## Rules

- A `range` lives anywhere: it holds iterators, never a `tracked_ptr` of its own. The elements it walks are the container's, and the container's rules on iterators apply: a node erased under an iterator invalidates it, as in `std`.
- The counting forms take one integer type for both ends (`range(size_t(4), size_t(6))`, not `range(4, size_t(6))`); a negative `last` in `range(last)` is empty.
- `front()` on an empty range is undefined, as `*begin()` is.

## Members

```cpp
using iterator = It;
using value_type = ...;                           // what *begin() yields, without the reference

range();                                          // empty
range(It first, It last) noexcept;
template<class Pair> range(Pair p) noexcept;      // from an equal_range pair (It from p.first)
template<std::integral T> explicit range(T last) noexcept;   // 0..last, only for It = detail::counter<T>
template<std::integral T> range(T first, T last) noexcept;   // first..last, empty when first > last

bool empty() const noexcept;
size_t size() const;                              // a subtraction for a random-access iterator, a walk for a forward one
decltype(auto) front() const noexcept;            // *begin(); the range must not be empty
It begin() const noexcept;
It end() const noexcept;
```

`std::ranges::enable_borrowed_range<sgcl::range<It>>` is `true`.

A `range` is a range of the library ([the mixins](mixin/README.md)): it carries `m_enumerable`, `m_equatable`, `m_comparable`, `m_ordered`, and, by what its iterator is, the category markers and `m_sequence` — so `range(v.begin(), v.end())` over a `std::vector` is how a standard container enters a function that asks for `c_enumerable` or `c_ordered`, and `range(n).contains(3)`, `range(n).max()` answer as a vector would.

```cpp
std::vector<int> sv = {3, 1, 2};
sgcl::range r(sv.begin(), sv.end());
r.sort();                                         // sv is 1 2 3
assert(r.is_sorted() && r.contains(2) && sgcl::range(5).max() == 4 && sgcl::range(5).count_of([](int x) { return x % 2; }) == 2);
static_assert(sgcl::c_contiguous<decltype(r)> && sgcl::c_sequence<decltype(r)> && !sgcl::c_sequence<sgcl::range<sgcl::detail::counter<int>>>);
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <algorithm>
#include <iostream>
#include <ranges>

int main() {
    int sum = 0;
    for (int i : sgcl::range(10)) {                 // 0..9
        sum += i;
    }
    std::cout << sum << "\n";                       // 45

    for (int i : sgcl::range(2, 5)) {               // 2, 3, 4
        std::cout << i << " ";
    }
    std::cout << "\n";
    std::cout << sgcl::range(7, 3).empty() << " " << sgcl::range(3, 8).size() << "\n";   // 1 5

    auto squares = sgcl::range(4) | std::views::transform([](int i) { return i * i; });
    std::cout << *std::ranges::max_element(squares) << "\n";   // 9

    sgcl::sorted_multimap<sgcl::string, int> scores = {{"ann", 90}, {"ann", 95}, {"bob", 70}};
    sgcl::range ann = scores.equal_range("ann");   // range<iterator>, deduced from the pair
    int best = 0;
    for (auto& [name, score] : ann) {
        best = std::max(best, score);
    }
    sgcl::range cid = scores.equal_range("cid");
    std::cout << ann.size() << " " << best << " " << cid.empty() << "\n";   // 2 95 1
    return 0;
}
```

The output:

```
45
2 3 4 
1 5
9
2 95 1
```

## See also

- [sorted_multimap](../containers/sorted_multimap.md), [multimap](../containers/multimap.md), [weak_map](../containers/weak_map.md): `equal_range`, the pair a `range` is made from
- [vector](../containers/vector.md), [the mixins](mixin/README.md): the containers and the algorithms a counted loop indexes into

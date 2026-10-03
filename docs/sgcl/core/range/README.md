[sgcl](../../README.md) › [core](../README.md)

# sgcl::range\<It\>

```cpp
#include "sgcl/core/range.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class It>
    class range;
}
```

`sgcl::range<It>` is a pair of iterators as a range, half-open, for a range-for and for `std::ranges`: what
`equal_range` of a [sorted_multimap](../sorted_multimap/README.md), a [multimap](../multimap/README.md) or a
[weak_multimap](../weak_multimap/README.md) hands back as a `std::pair`, made iterable (`range ones = m.equal_range(1)`). The same class counts integers:
`range(10)` is `0, 1, ..., 9`, `range(2, 10)` is `2, ..., 9`, the shape of Go's `for i := range 10` and C#'s
`Enumerable.Range`, over an iterator of the library that holds the number. That iterator is random access to
`std::ranges` through its `iterator_concept`, so `size()` is a subtraction; to the pre-C++20 `iterator_category` it
is an input iterator, as `std::ranges::iota_view`'s is, since `*i` is a value, which the old forward category
forbids.

A `range` holds two iterators and nothing else: it owns no elements, lives anywhere (it is not a managed type), and
is a borrowed range to `std::ranges` (`std::ranges::enable_borrowed_range` is `true`), so an algorithm may hand back
an iterator into a temporary (`*std::ranges::max_element(range(3, 8))`). It is `std::ranges::subrange` and
`std::views::iota` in one class, under one name; it lives in `core`, next to the aliases, because it needs nothing
from the containers and every module's examples count with it.

There is no step and no walk downwards: `range(first, last)` with `first > last` is empty, not a descent, and a
plain `for` says a step better than a third argument would.

A `range` is a range of the library ([the mixins](../mixin/README.md)): it carries `mixin::enumerable`,
`mixin::equatable`, `mixin::comparable` and `mixin::ordered`, and by what its iterator is, the category markers
(`bidirectional`, `random_access`, `contiguous`) and `mixin::sequence` when the iterator writes. So
`range(v.begin(), v.end())` over a `std::vector` is how a standard container enters a function that asks for
`req::enumerable`, `req::ordered` or `req::sequence`, and `range(n).contains(3)`, `range(n).max()` answer as a
vector would.

## Rules

- A `range` lives anywhere: it holds iterators, never a `tracked_ptr` of its own. The elements it walks are the
  container's, and the container's rules on iterators apply: a node erased under an iterator invalidates it, as in
  `std`.
- The counting forms take one integer type for both ends (`range(size_t(4), size_t(6))`, not
  `range(4, size_t(6))`); a negative `last` in `range(last)` is empty.
- `front()` on an empty range is undefined, as `*begin()` is.

## Template parameters

| Parameter | Description |
|---|---|
| `It` | The iterator: any iterator, of a container of the library or of `std`, or the [counting_iterator](../counting_iterator.md) the deduction guides of the integer forms give. |

## Member types

| Type | Definition |
|---|---|
| `iterator` | `It` |
| `value_type` | `std::iterator_traits<It>::value_type`: what `*begin()` yields, without the reference |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](range.md) | constructs a range of two iterators, of a pair, or of integers |
| [empty](empty.md) | checks whether the range is empty |
| [size](size.md) | the number of elements |
| [front](front.md) | the first element |

#### Iterators

| Function | Description |
|---|---|
| [begin](begin.md) | the iterator to the beginning |
| [end](end.md) | the iterator to the end |

#### From mixin::enumerable

The questions asked of the elements, carried by every range of the library
([mixin::enumerable](../mixin/enumerable/README.md)).

| Function | Description |
|---|---|
| [contains](../mixin/enumerable/contains.md) | checks whether an element is equal to a value |
| `index_of` | the position of the first element equal to a value |
| `last_index_of` | the position of the last element equal to a value |
| `find_if` | the first value the predicate accepts, in an `optional`: the counter gives values, not elements |
| `find_index` | the position of the first element the predicate accepts |
| `exists` | checks whether the predicate accepts some element |
| `all` | checks whether the predicate accepts every element |
| `count_of` | the number of elements the predicate accepts |
| `min`, `max` | the smallest, the largest element |
| `for_each` | calls a function with every element |

#### From mixin::ordered

The order of the elements ([mixin::ordered](../mixin/ordered/README.md)); the sorts only when the iterator writes and is
random access.

| Function | Description |
|---|---|
| `sort` | sorts the elements |
| `sort_by` | sorts the elements by a projection |
| `stable_sort` | sorts the elements, keeping the order of equal ones |
| `is_sorted` | checks whether the elements are sorted |
| `binary_search` | checks whether a sorted range holds a value |
| `lower_bound`, `upper_bound` | the first element not less than, greater than a value, in a sorted range |
| `sorted_index_of` | the position of a value in a sorted range |

#### From mixin::sequence

The writes over every element, when the iterator writes ([mixin::sequence](../mixin/sequence/README.md)).

| Function | Description |
|---|---|
| `fill` | assigns a value to every element |
| `reverse` | reverses the order of the elements |

## Non-member functions

| Function | Description |
|---|---|
| `operator==`, `operator<=>` | compare the elements lexicographically ([mixin::equatable](../mixin/equatable/README.md), [mixin::comparable](../mixin/comparable/README.md)) |

## Deduction guides

```cpp
template<std::integral T>
range(T last) -> range<counting_iterator<T>>;

template<std::integral T>
range(T first, T last) -> range<counting_iterator<T>>;

template<class Pair>
range(Pair p) -> range<decltype(p.first)>;
```

The first two make the counting forms, over [counting_iterator](../counting_iterator.md), which holds a `T`; the third
takes the iterator type of an `equal_range` pair.

## Complexity

Every member function is constant, but `size()` over an iterator that is not random access, which walks the range,
and the mixins' questions, which are linear in the number of elements (logarithmic for the binary searches over a
random-access range).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <algorithm>
#include <ranges>
#include <vector>

using namespace sgcl;

int main() {
    int sum = 0;
    for (int i : range(10)) {
        sum += i;
    }
    println("{} {}", sum, range(2, 5));

    auto squares = range(4) | std::views::transform([](int i) { return i * i; });
    println("{}", *std::ranges::max_element(squares));

    sorted_multimap<string, int> scores = {{"ann", 90}, {"ann", 95}, {"bob", 70}};
    range ann = scores.equal_range("ann");  // range<iterator>, deduced from the pair
    int best = 0;
    for (auto& [name, score] : ann) {
        best = std::max(best, score);
    }
    println("{} {} {}", ann.size(), best, range(scores.equal_range("cid")).empty());

    std::vector<int> sv = {3, 1, 2};
    range all(sv.begin(), sv.end());  // a std container as a range of the library
    all.sort();
    println("{} {} {}", sv, range(5).max(), range(5).count_of([](int x) { return x % 2 != 0; }));
}
```

Output:

```text
45 [2, 3, 4]
9
2 95 true
[1, 2, 3] 4 2
```

## See also

- [sorted_multimap](../sorted_multimap/README.md), [multimap](../multimap/README.md), [weak_multimap](../weak_multimap/README.md): `equal_range`,
  the pair a `range` is made from
- [vector](../vector/README.md), [the mixins](../mixin/README.md): the containers and the algorithms a counted loop indexes into
- [slice](../slice/README.md): a view of contiguous elements that holds their buffer
- [counting_iterator](../counting_iterator.md): the iterator of the counting forms

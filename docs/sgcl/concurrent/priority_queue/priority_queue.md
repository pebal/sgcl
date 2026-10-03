[sgcl](../../README.md) › [concurrent](../README.md) › [priority_queue](README.md)

# sgcl::concurrent::priority_queue\<T, Compare\>::priority_queue

```cpp
priority_queue()                                                                    // (1)
    noexcept(std::is_nothrow_default_constructible_v<Compare> &&
             std::is_nothrow_copy_constructible_v<Compare>);
explicit priority_queue(const Compare& comp)                                        // (2)
    noexcept(std::is_nothrow_copy_constructible_v<Compare>);
template<std::input_iterator InputIt>
priority_queue(InputIt first, InputIt last, const Compare& comp = Compare());       // (3)
priority_queue(std::initializer_list<T> ilist, const Compare& comp = Compare());    // (4)
priority_queue(const priority_queue&) = delete;                                     // (5)
```

Constructs a queue from one of the sources below.

1. An empty queue with a default-constructed comparator. Nothing is allocated until the first push.
2. An empty queue with a copy of `comp`.
3. A queue holding the elements of the range `[first, last)`, pushed one by one in their order, so that equal
   elements come out in the order of the range.
4. A queue holding the elements of `ilist`, pushed as in (3).
5. The queue is not copyable, and not movable: a structure shared by threads has one place.

## Parameters

| Parameter | Description |
|---|---|
| `comp` | the comparator: `comp(a, b)` is `true` when `a` comes out before `b` |
| `first`, `last` | the range of the elements to push |
| `ilist` | the list of the elements to push |

## Complexity

- (1–2) Constant.
- (3) Linear in the distance between `first` and `last`, a push each: up to *n* log *n* comparisons.
- (4) Linear in `ilist.size()`, a push each: up to *n* log *n* comparisons.

## Exceptions

- (1–2) What the construction of `Compare` (its default constructor, the copy of `comp`) throws; none when it is
  noexcept.
- (3–4) What the copy of `comp` or the construction of `T` from an element of the range throws.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <functional>

using namespace sgcl;

int main() {
    concurrent::priority_queue<int> smallest = {3, 1, 2};
    concurrent::priority_queue<int, std::greater<int>> largest = {3, 1, 2};
    println("{} {}", *smallest.try_top(), *largest.try_top());

    auto shorter = [](const string& a, const string& b) noexcept { return a.size() < b.size(); };
    vector<string> words = {"three", "two", "one", "four"};
    concurrent::priority_queue<string, decltype(shorter)> by_length(words.begin(), words.end(),
                                                                    shorter);
    while (auto w = by_length.try_pop()) {
        println("{}", *w);
    }
}
```

Output:

```text
1 3
two
one
four
three
```

## See also

- [push](push.md), [emplace](emplace.md): insert an element
- [sgcl::concurrent::priority_queue\<T, Compare\>](README.md)

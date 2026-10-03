[sgcl](../../README.md) › [core](../README.md) › [list](../list.md)

# sgcl::list\<T\>::list

```cpp
/*(1)*/ list() noexcept;
/*(2)*/ explicit list(size_type count) noexcept(std::is_nothrow_default_constructible_v<T>);
/*(3)*/ list(size_type count, const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>);
/*(4)*/ template<std::input_iterator InputIt> list(InputIt first, InputIt last);
/*(5)*/ template<std::ranges::input_range R> explicit list(R&& r);
/*(6)*/ list(std::initializer_list<T> ilist);
/*(7)*/ list(const list& other);
/*(8)*/ list(list&& other) noexcept;
```

Constructs a list from one of the sources below.

1. An empty list. Nothing is allocated, not even the sentinel: it is made by the first insertion.
2. `count` value-initialized elements.
3. `count` copies of `value`.
4. The elements of the range `[first, last)`.
5. The elements of the range `r`, each made from what the range gives: the pieces of a string, a view, another
   container. Takes part only when `T` is constructible from the elements of `r` and `r` is not a list of the same
   type, which is the copy (7). The counterpart of `std::from_range` of C++23.
6. The elements of `ilist`.
7. A copy of `other`, in nodes of its own.
8. Takes the sentinel of `other` over, with every node; `other` is empty after, without a sentinel.

## Parameters

| Parameter | Description |
|---|---|
| `count` | the number of elements |
| `value` | the value the elements are copied from |
| `first`, `last` | the range the elements are copied from |
| `r` | the range the elements are made from |
| `ilist` | the list the elements are copied from |
| `other` | the list the elements are copied or taken from |

## Complexity

- (1) Constant.
- (2–3) Linear in `count`.
- (4) Linear in the distance between `first` and `last`.
- (5) Linear in the size of `r`.
- (6) Linear in the size of `ilist`.
- (7) Linear in `other.size()`.
- (8) Constant.

## Exceptions

- (1), (8) None.
- (2–7) What the constructor of `T` throws; none for (2) and (3) when it is noexcept.

The new elements are made in a chain of nodes first and linked in at once: when an element's constructor throws,
the elements built before it are destroyed and the exception propagates, and no node holds an element that was
never constructed.

## Notes

The nodes are on the managed heap. A move (8) takes them over without touching an element, as `std::list` does;
nothing is copied and nothing is freed.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <vector>

using namespace sgcl;

int main() {
    list<int> empty;
    list<int> zeros(4);
    list<string> words(2, "x");
    list digits = {1, 2, 3};

    std::vector<double> halves = {0.5, 1.5};
    list<double> from_vector(halves.begin(), halves.end());

    list<string> pieces(string("a,b,c").split(','));
    list copy = digits;
    list taken = std::move(digits);

    println("{} {} {} {}", empty, zeros, words, from_vector);
    println("{} {} {} {}", pieces, copy, taken, digits.size());
}
```

Output:

```text
[] [0, 0, 0, 0] ["x", "x"] [0.5, 1.5]
["a", "b", "c"] [1, 2, 3] [1, 2, 3] 0
```

## See also

- [operator=](operator_assign.md), [assign](assign.md): replace the contents of a list
- [insert](insert.md): inserts elements at a position
- [sgcl::list\<T\>](../list.md)

[sgcl](../../README.md) › [core](../README.md) › [forward_list](../forward_list.md)

# sgcl::forward_list\<T\>::forward_list

```cpp
forward_list() noexcept;                                                                       // (1)
explicit forward_list(size_type count) noexcept(std::is_nothrow_default_constructible_v<T>)    // (2)
    requires std::default_initializable<T>;
forward_list(size_type count, const T& value)                                                  // (3)
    noexcept(std::is_nothrow_copy_constructible_v<T>);
template<std::input_iterator InputIt> forward_list(InputIt first, InputIt last);               // (4)
template<std::ranges::input_range R> explicit forward_list(R&& r);                             // (5)
forward_list(std::initializer_list<T> ilist);                                                  // (6)
forward_list(const forward_list& other);                                                       // (7)
forward_list(forward_list&& other) noexcept;                                                   // (8)
```

Constructs a list from one of the sources below. The sentinel is inside the list object: an empty list allocates
nothing.

1. An empty list.
2. `count` value-initialized elements.
3. `count` copies of `value`.
4. The elements of the range `[first, last)`.
5. The elements of the range `r`, each made from what the range gives: the pieces of a string, a view, another
   container. Takes part only when `T` is constructible from the elements of `r` and `r` is not a forward_list of
   the same type, which is the copy (7). The counterpart of `std::from_range` of C++23.
6. The elements of `ilist`.
7. A copy of `other`, in nodes of its own.
8. Takes the chain of nodes of `other` over; `other` keeps its sentinel and is empty after.

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
- (7) Linear in the number of elements of `other`.
- (8) Constant.

## Exceptions

- (1), (8) None.
- (2–7) What the constructor of `T` throws; none for (2) and (3) when it is noexcept.

When an element's constructor throws, the elements built before it are destroyed and the exception propagates: no
node holds an element that was never constructed.

## Notes

The nodes are on the managed heap. A move (8) takes them over without touching an element, as
`std::forward_list` does; nothing is copied and nothing is freed.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <vector>

using namespace sgcl;

int main() {
    forward_list<int> empty;
    forward_list<int> zeros(4);
    forward_list<string> words(2, "x");
    forward_list digits = {1, 2, 3};

    std::vector<double> halves = {0.5, 1.5};
    forward_list<double> from_vector(halves.begin(), halves.end());

    forward_list<string> pieces(string("a,b,c").split(','));
    forward_list copy = digits;
    forward_list taken = std::move(digits);

    println("{} {} {} {}", empty, zeros, words, from_vector);
    println("{} {} {} {}", pieces, copy, taken, digits.empty());
}
```

Output:

```text
[] [0, 0, 0, 0] ["x", "x"] [0.5, 1.5]
["a", "b", "c"] [1, 2, 3] [1, 2, 3] true
```

## See also

- [operator=](operator_assign.md), [assign](assign.md): replace the contents of a list
- [insert_after](insert_after.md): inserts elements after a position
- [sgcl::forward_list\<T\>](../forward_list.md)

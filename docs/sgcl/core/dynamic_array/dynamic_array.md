[sgcl](../../README.md) › [core](../README.md) › [dynamic_array](../dynamic_array.md)

# sgcl::dynamic_array\<T\>::dynamic_array

```cpp
dynamic_array() noexcept = default;                                                  // (1)
explicit dynamic_array(size_type count);                                             // (2)
dynamic_array(size_type count, const T& value);                                      // (3)
template<std::input_iterator InputIt> dynamic_array(InputIt first, InputIt last);    // (4)
template<std::ranges::input_range R> explicit dynamic_array(R&& r);                  // (5)
dynamic_array(std::initializer_list<T> ilist);                                       // (6)
dynamic_array(const dynamic_array& other);                                           // (7)
dynamic_array(dynamic_array&& other) noexcept;                                       // (8)
```

Constructs an array from one of the sources below. Its size is fixed from then on.

1. An empty array that holds no buffer. Nothing is allocated.
2. `count` value-initialized elements. For `tracked_ptr` elements the buffer is already zeroed, so they are null
   pointers without a constructor run; for elements whose value-initialized form is all zero bits (`int`, a
   struct of them) the buffer is zeroed in one pass; any other type is constructed element by element.
3. `count` copies of `value`.
4. The elements of the range `[first, last)`. A forward range is counted first and the buffer allocated once; a
   single-pass range is collected into a [vector](../vector.md) first and moved from there.
5. The elements of the range `r`, each made from what the range gives: the pieces of a string, a view, another
   container; as (4) over the range's iterators. Takes part only when `T` is constructible from the elements of
   `r` and `r` is not a dynamic_array of the same type, which is the copy (7). The counterpart of
   `std::from_range` of C++23.
6. The elements of `ilist`.
7. A copy of `other`, in a buffer of its own.
8. Takes the buffer of `other` over; `other` is empty after.

## Parameters

| Parameter | Description |
|---|---|
| `count` | the number of elements |
| `value` | the value the elements are copied from |
| `first`, `last` | the range the elements are copied from |
| `r` | the range the elements are made from |
| `ilist` | the list the elements are copied from |
| `other` | the array the elements are copied or taken from |

## Complexity

- (1) Constant.
- (2) Linear in `count`; constant for `tracked_ptr` elements.
- (3) Linear in `count`.
- (4) Linear in the distance between `first` and `last`.
- (5) Linear in the size of `r`.
- (6) Linear in the size of `ilist`.
- (7) Linear in `other.size()`.
- (8) Constant.

## Exceptions

- (1), (8) None.
- (2–7) `length_error` when the number of elements is above `max_size()`, and what the constructor of `T`
  throws.

When an element's constructor throws, the elements built before it are destroyed and the exception propagates:
no constructor leaves elements alive and unaccounted.

## Notes

The buffer is on the managed heap, and the size class it comes from may give it more room than was asked for; the
count is the handle's, and the room past it is never used. A move (8) takes the buffer over without touching an
element: nothing is copied and nothing is freed.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <sstream>

using namespace sgcl;

int main() {
    dynamic_array<int> empty;
    dynamic_array<int> zeros(5);
    dynamic_array<string> names(3, "n");
    dynamic_array<int> digits = {1, 2, 3};

    vector src = {4, 5, 6, 7};
    dynamic_array from(src.begin(), src.end());  // deduced: dynamic_array<int>
    dynamic_array<double> halves(src);  // from the vector as a range

    std::istringstream in("8 9");
    dynamic_array<int> read(std::istream_iterator<int>(in), std::istream_iterator<int>{});

    dynamic_array copy = digits;
    dynamic_array<int> taken = std::move(digits);  // digits is empty now
    dynamic_array<tracked_ptr<int>> ptrs(10);  // ten null pointers, in a managed buffer

    println("{} {} {} {} {}", empty, zeros, names, from, read);
    println("{} {} {} {}", copy, taken, digits.size(), ptrs[9] == nullptr);
    println("{}", halves);
}
```

Output:

```text
[] [0, 0, 0, 0, 0] ["n", "n", "n"] [4, 5, 6, 7] [8, 9]
[1, 2, 3] [1, 2, 3] 0 true
[4, 5, 6, 7]
```

## See also

- [operator=](operator_assign.md): replaces the contents of an array
- [sgcl::dynamic_array\<T\>](../dynamic_array.md)

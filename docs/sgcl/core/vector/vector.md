[sgcl](../../README.md) › [core](../README.md) › [vector](../vector.md)

# sgcl::vector\<T\>::vector

```cpp
/*(1)*/ vector() noexcept;
/*(2)*/ explicit vector(size_type count);
/*(3)*/ vector(size_type count, const T& value);
/*(4)*/ template<std::input_iterator InputIt> vector(InputIt first, InputIt last);
/*(5)*/ template<std::ranges::input_range R> explicit vector(R&& r);
/*(6)*/ vector(std::initializer_list<T> ilist);
/*(7)*/ vector(const vector& other);
/*(8)*/ vector(vector&& other) noexcept;
```

Constructs a vector from one of the sources below.

1. An empty vector. Nothing is allocated.
2. `count` value-initialized elements. For elements whose value-initialized form is all zero bits (`int`,
   `tracked_ptr`, a struct of them) the buffer is zeroed and no constructor runs.
3. `count` copies of `value`.
4. The elements of the range `[first, last)`. A forward range is counted first and the buffer allocated once; a
   single-pass range is appended element by element.
5. The elements of the range `r`, each made from what the range gives: the pieces of a string, a view, another
   container. Takes part only when `T` is constructible from the elements of `r` and `r` is not a vector of the
   same type, which is the copy (7). The counterpart of `std::from_range` of C++23.
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
| `other` | the vector the elements are copied or taken from |

## Complexity

- (1) Constant.
- (2) Linear in `count`; constant for elements that are all zero bits.
- (3) Linear in `count`.
- (4) Linear in the distance between `first` and `last`.
- (5) Linear in the size of `r`.
- (6) Linear in the size of `ilist`.
- (7) Linear in `other.size()`.
- (8) Constant.

## Exceptions

- `length_error` when the number of elements is above `max_size()`.
- What the constructor of `T` throws.

When an element's constructor throws, the elements built before it are destroyed and the exception propagates:
no constructor leaves elements alive and unaccounted.

## Notes

The buffer is on the managed heap. A move (8) takes it over without touching an element, as `std::vector` does;
nothing is copied and nothing is freed.

The capacity after (2)–(7) may be above the number of elements: it is what the size class of the buffer holds
([reserve](reserve.md)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <list>

using namespace sgcl;

int main() {
    vector<int> empty;
    vector<int> zeros(3);
    vector<string> words(2, "hi");
    vector digits = {1, 2, 3};

    std::list<double> halves = {0.5, 1.5};
    vector from_list(halves.begin(), halves.end());  // deduced: vector<double>

    vector<string> pieces(string("a,b,c").split(','));
    vector copy = digits;
    vector taken = std::move(digits);

    println("{} {} {} {}", empty, zeros, words, from_list);
    println("{} {} {} {}", pieces, copy, taken, digits.size());
}
```

Output:

```text
[] [0, 0, 0] ["hi", "hi"] [0.5, 1.5]
["a", "b", "c"] [1, 2, 3] [1, 2, 3] 0
```

## See also

- [operator=](operator_assign.md), [assign](assign.md): replace the contents of a vector
- [reserve](reserve.md): a buffer for the elements to come
- [sgcl::vector\<T\>](../vector.md)

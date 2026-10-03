[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::deque

```cpp
/*(1)*/ deque() noexcept;
/*(2)*/ explicit deque(size_type count) requires std::default_initializable<T>;
/*(3)*/ deque(size_type count, const T& value);
/*(4)*/ template<std::input_iterator InputIt> deque(InputIt first, InputIt last);
/*(5)*/ template<std::ranges::input_range R> explicit deque(R&& r);
/*(6)*/ deque(std::initializer_list<T> ilist);
/*(7)*/ deque(const deque& other);
/*(8)*/ deque(deque&& other) noexcept;
```

Constructs a deque from one of the sources below.

1. An empty deque. Nothing is allocated.
2. `count` value-initialized elements.
3. `count` copies of `value`.
4. The elements of the range `[first, last)`, appended element by element, single-pass ranges included.
5. The elements of the range `r`, each made from what the range gives: the pieces of a string, a view, another
   container. Takes part only when `T` is constructible from the elements of `r` and `r` is not a deque of the
   same type, which is the copy (7). The counterpart of `std::from_range` of C++23.
6. The elements of `ilist`.
7. A copy of `other`, in blocks of its own.
8. Takes the map of `other` over; `other` is empty after.

## Parameters

| Parameter | Description |
|---|---|
| `count` | the number of elements |
| `value` | the value the elements are copied from |
| `first`, `last` | the range the elements are copied from |
| `r` | the range the elements are made from |
| `ilist` | the list the elements are copied from |
| `other` | the deque the elements are copied or taken from |

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
- (2–3) `length_error` when `count > max_size()`.
- (2–7) What the constructor of `T` throws.

When an element's constructor throws, the elements built before it are destroyed and the exception propagates:
no constructor leaves elements alive and unaccounted.

## Notes

The blocks and the map are on the managed heap. A move (8) takes the map over without touching an element, as
`std::deque` does; nothing is copied and nothing is freed.

A pair of iterators deduces the element type, as for a `vector`: `deque d(first, last)`; so does a list,
`deque digits = {1, 2, 3}`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <list>

using namespace sgcl;

int main() {
    deque<int> empty;
    deque<int> zeros(3);
    deque<string> words(2, "hi");
    deque digits = {1, 2, 3};

    std::list<double> halves = {0.5, 1.5};
    deque from_list(halves.begin(), halves.end());  // a deque<double>

    deque<string> pieces(string("a,b,c").split(','));
    deque copy = digits;
    deque taken = std::move(digits);

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

- [operator=](operator_assign.md), [assign](assign.md): replace the contents of a deque
- [resize](resize.md): changes the number of elements
- [sgcl::deque\<T\>](../deque.md)

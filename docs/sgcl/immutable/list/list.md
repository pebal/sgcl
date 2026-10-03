[sgcl](../../README.md) › [immutable](../README.md) › [list](../list.md)

# sgcl::immutable::list\<T\>::list

```cpp
/*(1)*/ list() noexcept;
/*(2)*/ template<std::input_iterator InputIt> list(InputIt first, InputIt last)
            noexcept(/* see below */);
/*(3)*/ list(std::initializer_list<T> ilist) noexcept(std::is_nothrow_copy_constructible_v<T>);
/*(4)*/ template<std::ranges::input_range R> explicit list(R&& r) noexcept(/* see below */);
/*(5)*/ list(const list& other) noexcept;
/*(6)*/ list(list&& other) noexcept;
```

Constructs a list from one of the sources below.

1. An empty list. It holds no cell.
2. The elements of the range `[first, last)`, in its order. The cells are made from the back, one allocation
   each: a bidirectional range is walked backwards; a range read only forward (a `forward_list`, a stream) is
   first collected into a [vector](../../core/vector.md), then taken from its back.
3. The elements of `ilist`, in its order.
4. The elements of the range `r`, each made from what the range gives: the pieces of a string, a view, another
   container. Takes part only when `T` is constructible from the elements of `r` and `r` is not a list of the
   same type, which is the copy (5).
5. The version `other` holds: a copy of its two words, every cell shared.
6. The same as (5): `other` keeps its version too.

## Parameters

| Parameter | Description |
|---|---|
| `first`, `last` | the range the elements are made from |
| `ilist` | the list the elements are copied from |
| `r` | the range the elements are made from |
| `other` | the list whose version is taken |

## Complexity

- (1) Constant.
- (2) Linear in the distance between `first` and `last`.
- (3) Linear in the size of `ilist`.
- (4) Linear in the size of `r`.
- (5–6) Constant.

## Exceptions

- (1), (5–6) None.
- (2), (4) What the walk over the range (`begin` and `end` of `r`, the copy, the comparison, the increment and the
  dereference of the iterator), the constructor of `T` from an element and, for a range read only forward, the
  move constructor of `T` throw; none when they are noexcept.
- (3) What the copy constructor of `T` throws; none when it is noexcept.

When an element's constructor throws, the cells made so far are left to the collector, which destroys their
elements; the exception propagates and no list is made.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::list<int> empty;
    immutable::list<int> digits = {1, 2, 3};

    vector<double> halves = {0.5, 1.5};
    immutable::list from_range(halves.begin(), halves.end());  // deduced: list<double>

    immutable::list<string> pieces(string("a,b,c").split(','));
    immutable::list copy = digits;  // two words: the cells are shared
    println("{} {} {} {} {}", empty, digits, from_range, pieces, copy);
}
```

Output:

```text
[] [1, 2, 3] [0.5, 1.5] ["a", "b", "c"] [1, 2, 3]
```

## See also

- [operator=](operator_assign.md): makes the variable hold another version
- [push_front](push_front.md): the list with one more element in front
- [sgcl::immutable::list\<T\>](../list.md)

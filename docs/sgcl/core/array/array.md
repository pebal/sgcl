[sgcl](../../README.md) › [core](../README.md) › [array](README.md)

# sgcl::array\<T, N\>::array

```cpp
constexpr array() = default;                                                               // (1)
constexpr array(T... elements) noexcept(std::is_nothrow_move_constructible_v<T>);          // (2)
template<class... U>
requires (sizeof...(U) >= 1 && sizeof...(U) <= N && (std::convertible_to<U, T> && ...))
constexpr array(U&&... elements) noexcept(/* see below */);                                // (3)
```

Constructs an array, with the braces of an aggregate written as a constructor.

1. The elements default-initialized, as an aggregate's: the constructor is trivial, and `array<int, 4> raw;`
   leaves the four `int`s uninitialized, as `int[4]` does. `= {}` value-initializes them: zeros, null pointers.
2. For `N` up to 64: `N` parameters of type `T`, one per element, each moved into its element. A parameter of
   type `T` takes what it would take: a brace for an element that is itself an aggregate,
   `array<point, 2> p = {{1, 2}, {3, 4}}`, one level for one level; `array<double, 2> d = {1, 2}`; constants that
   fit, as `array<uint8_t, 2> b = {1, 0xff}`; a narrowing one is refused.
3. Fewer than `N` arguments, and past 64 elements `N` of them: arguments of any types that convert to `T`
   implicitly, each converted as a parameter of type `T` would be. The same arguments as (2), but a brace without a
   type is refused (`{{1, 2}, ...}` for points; `{point{1, 2}, ...}` is not), and a narrowing one is not (`1.5` for
   an `int`). The elements past the arguments are value-initialized, as an aggregate's:
   `array<char, 7> css = {'#'}` is `'#'` and six zeros.

- (2–3) Each element is constructed in place from its argument; an element that only moves is moved in. The count
  may not pass `N`: a fourth argument for an `array<T, 3>` is an error at compile time. `= {}` zeroes every
  element, through (1).

The copy and the move constructors are implicitly declared: element by element, trivial for a trivial `T`.

## Parameters

| Parameter | Description |
|---|---|
| `elements` | the values of the elements, the first element's first |

## Complexity

- (1) Constant for a trivial `T`; otherwise linear in `N`, a default constructor per element.
- (2–3) Linear in `N`.

## Exceptions

- (1) What the default constructor of `T` throws; none for a trivial `T`.
- (2) What the copy or the move of `T` into a parameter throws, and its move into the element; none from that move
  when it is noexcept.
- (3) What the construction of `T` from an argument throws, and with fewer than `N` arguments the
  value-initialization of the rest; none when every one is noexcept.

When an element's constructor throws, the elements built before it are destroyed and the exception propagates.

## Notes

Neither form puts anything but `T` and `N` into the type of the array: `array<std::byte, 32768>` has a name of a
few dozen characters. That is why the elements past 64 are taken by a template: the `N` parameters of type `T`
of (2) come from an index sequence of `N`, which a large `N` would instantiate and carry in its debug information.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <memory>
#include <type_traits>

using namespace sgcl;

struct Point {
    int x, y;
};

int main() {
    array<int, 3> a = {1, 2, 3};
    array<tracked_ptr<int>, 2> roots = {};  // two null pointers, on the stack
    roots[0] = make_tracked<int>(5);
    array<Point, 2> points = {{1, 2}, {3, 4}};
    array<double, 2> d = {1, 2};
    array<std::unique_ptr<int>, 2> owned = {std::make_unique<int>(1), std::make_unique<int>(2)};
    array<int, 3> zeros = {};
    array<int, 4> first_two = {1, 2};  // the rest value-initialized

    println("{} {} {} {} {}", a, *roots[0], roots[1] == nullptr, points[1].y, d);
    println("{} {} {}", *owned[1], zeros, first_two);

    constexpr array<int, 2> c = {4, 5};  // constexpr: everything is
    constexpr bool has_four = c.contains(4);
    println("{} {} {}", c[1], c.size(), has_four);

    println("{} {}", std::is_trivially_default_constructible_v<array<int, 4>>,
            sizeof(array<int, 4>) == 4 * sizeof(int));
}
```

Output:

```text
[1, 2, 3] 5 true 4 [1, 2]
2 [0, 0, 0] [1, 2, 0, 0]
5 2 true
true true
```

## See also

- [to_array](../to_array.md): an array from a built-in array
- [fill](fill.md): assigns a value to every element
- [sgcl::array\<T, N\>](README.md)

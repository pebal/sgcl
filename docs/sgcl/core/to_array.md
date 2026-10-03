[sgcl](../README.md) › [core](README.md)

# sgcl::to_array

```cpp
#include "sgcl/core/array.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T, size_t N>
    constexpr array<std::remove_cv_t<T>, N> to_array(T (&a)[N])                 // (1)
        noexcept(std::is_nothrow_constructible_v<std::remove_cv_t<T>, T&> &&
                 std::is_nothrow_move_constructible_v<std::remove_cv_t<T>>);
    template<class T, size_t N>
    constexpr array<std::remove_cv_t<T>, N> to_array(T (&&a)[N])                // (2)
        noexcept(std::is_nothrow_move_constructible_v<std::remove_cv_t<T>>);
}
```

Builds an [array](array/README.md) from a built-in array, with the count and the type of the elements deduced from it,
as `std::to_array` does.

1. Copies the elements of `a`: an array from a named built-in array, or from a string literal, whose
   terminating `'\0'` is an element too.
2. Moves the elements of `a`: an array from a braced list, `to_array({3, 2, 1})`, or from a built-in array
   passed with `std::move`; elements that only move may be taken this way.

A `const` or `volatile` on the elements is dropped: `to_array` of a `const int[3]` is an `array<int, 3>`.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the built-in array the elements are copied or moved from |

## Return value

An `array<std::remove_cv_t<T>, N>` holding the elements of `a`, in their order.

## Complexity

Linear in `N`.

## Exceptions

- (1) What the copy constructor of `T` throws, and its move constructor; none when they are noexcept.
- (2) What the move constructor of `T` throws; none when it is noexcept.

## Notes

For elements of a type of `std`, `to_array(a)` written without a namespace finds `std::to_array` as well, by the
elements' type, and the call is ambiguous: `sgcl::to_array(a)` names this one.

From a braced list the count is deduced, and the type of the elements may be named while it is:
`to_array<long>({1, 2})` is an `array<long, 2>`, which the deduction guide of `array` cannot give
(`array fixed = {1, 2}` is an `array<int, 2>`).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <memory>
#include <type_traits>
#include <utility>

using namespace sgcl;

int main() {
    auto digits = to_array({3, 2, 1});  // array<int, 3>
    println("{} {}", digits, std::is_same_v<decltype(digits), array<int, 3>>);

    const int primes[] = {2, 3, 5, 7};
    auto copied = to_array(primes);  // array<int, 4>: the const is dropped
    copied[0] = 1;
    println("{}", copied);

    auto letters = to_array("abc");  // the '\0' too
    println("{}", letters.size());

    auto longs = to_array<long>({1, 2});
    println("{}", std::is_same_v<decltype(longs), array<long, 2>>);

    std::unique_ptr<int> owned[] = {std::make_unique<int>(4), std::make_unique<int>(5)};
    auto moved = sgcl::to_array(std::move(owned));  // std::to_array is found too
    println("{} {}", *moved[1], owned[1] == nullptr);
}
```

Output:

```text
[3, 2, 1] true
[1, 3, 5, 7]
4
true
5 true
```

## See also

- [array](array/README.md): the elements inline, with the braces of an aggregate
- [sgcl::array\<T, N\>](array/README.md)

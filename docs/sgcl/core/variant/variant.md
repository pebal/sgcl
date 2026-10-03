[sgcl](../../README.md) › [core](../README.md) › [variant](README.md)

# sgcl::variant\<Ts...\>::variant

```cpp
variant() noexcept(std::is_nothrow_default_constructible_v<T_0>)                         // (1)
    requires std::is_default_constructible_v<T_0>;
variant(const variant& o) noexcept((std::is_nothrow_copy_constructible_v<Ts> && ...))    // (2)
    requires (std::is_copy_constructible_v<Ts> && ...);
variant(variant&& o) noexcept((std::is_nothrow_move_constructible_v<Ts> && ...))         // (3)
    requires (std::is_move_constructible_v<Ts> && ...);
template<class U>
variant(U&& u) noexcept(std::is_nothrow_constructible_v<T_j, U>);                        // (4)
template<class T, class... A>
requires std::is_constructible_v<T, A...>
explicit variant(std::in_place_type_t<T>, A&&... a)                                      // (5)
    noexcept(std::is_nothrow_constructible_v<T, A...>);
template<class T, class U, class... A>
requires std::is_constructible_v<T, std::initializer_list<U>&, A...>
explicit variant(std::in_place_type_t<T>, std::initializer_list<U> il, A&&... a)         // (6)
    noexcept(std::is_nothrow_constructible_v<T, std::initializer_list<U>&, A...>);
template<size_t I, class... A>
requires std::is_constructible_v<T_I, A...>
explicit variant(std::in_place_index_t<I>, A&&... a)                                     // (7)
    noexcept(std::is_nothrow_constructible_v<T_I, A...>);
template<size_t I, class U, class... A>
requires std::is_constructible_v<T_I, std::initializer_list<U>&, A...>
explicit variant(std::in_place_index_t<I>, std::initializer_list<U> il, A&&... a)        // (8)
    noexcept(std::is_nothrow_constructible_v<T_I, std::initializer_list<U>&, A...>);
```

Constructs a variant. `T_I` is the alternative at index `I`, `variant_alternative_t<I, variant>`.

1. The first alternative, `T_0`, value-initialized.
2. The alternative `o` holds, copied; valueless when `o` is.
3. The alternative `o` holds, moved; valueless when `o` is. `o` keeps its index and a moved-from alternative.
4. The alternative `T_j` that the overload resolution of `std::variant` selects for `u`: an imaginary function per
   alternative that takes it, kept only when `T_j x[] = {std::forward<U>(u)}` does not narrow, so
   `variant<string, bool> v = "abc"` holds the `string` and `variant<long, double> v = 1` the `long`. Takes part
   only when `U` is neither the variant nor an `in_place` tag and the selection finds exactly one alternative.
5. The alternative `T`, constructed from `a...`. Takes part only when `T` is exactly one of `Ts`.
6. The same, from `il` and `a...`.
7. The alternative at index `I`, constructed from `a...`. Takes part only when `I` is below the number of
   alternatives.
8. The same, from `il` and `a...`.

The alternative is constructed in its place by its kind: a pointer word in the shared word, an alternative that
may hold pointers in a place of its own, any other in the shared data storage.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the variant to copy or to move from |
| `u` | the value the selected alternative is constructed from |
| `a` | the arguments the alternative is constructed from |
| `il` | the initializer list the alternative is constructed from |

## Complexity

Constant, plus the construction of the alternative.

## Exceptions

- (1) What the default constructor of `T_0` throws; none when it is noexcept.
- (2–3) What the copy or the move constructor of the alternative throws; none when every alternative's is
  noexcept.
- (4–8) What the constructor of the alternative throws; none when it is noexcept.

If an exception is thrown, no variant is made.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <utility>

using namespace sgcl;

struct Node {
    int value;
};

int main() {
    variant<int, tracked_ptr<Node>> empty;  // the int, 0
    variant<int, tracked_ptr<Node>> pointer = make_tracked<Node>(1);  // the tracked_ptr
    variant<string, bool> text = "abc";  // the string, not the bool
    variant<long, double> number = 1;  // the long: an int to a double would narrow
    variant<int, string> by_type(std::in_place_type<string>, 3, 'x');
    variant<int, vector<int>> by_index(std::in_place_index<1>, {1, 2, 3});
    variant<int, vector<int>> copy = by_index;

    println("{} {} {} {}", empty.index(), pointer.index(), text.index(), number.index());
    println("{} {} {}", get<1>(pointer)->value, get<string>(by_type), get<1>(copy));
}
```

Output:

```text
0 1 0 0
1 xxx [1, 2, 3]
```

## See also

- [operator=](operator_assign.md): assigns another variant or a value
- [emplace](emplace.md): constructs an alternative in place
- [sgcl::variant\<Ts...\>](README.md)

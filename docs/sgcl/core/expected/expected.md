[sgcl](../../README.md) › [core](../README.md) › [expected](../expected.md)

# sgcl::expected\<T, E\>::expected

```cpp
/*(1)*/  expected() noexcept(std::is_nothrow_default_constructible_v<T>)
             requires std::is_default_constructible_v<T>;
/*(2)*/  expected(const expected&) = default;
/*(3)*/  expected(expected&&) = default;
/*(4)*/  template<class U, class G>
         requires std::is_constructible_v<T, const U&> && std::is_constructible_v<E, const G&>
         explicit(!std::is_convertible_v<const U&, T> || !std::is_convertible_v<const G&, E>)
         expected(const expected<U, G>& o)
             noexcept(std::is_nothrow_constructible_v<T, const U&> &&
                      std::is_nothrow_constructible_v<E, const G&>);
/*(5)*/  template<class U, class G>
         requires std::is_constructible_v<T, U> && std::is_constructible_v<E, G>
         explicit(!std::is_convertible_v<U, T> || !std::is_convertible_v<G, E>)
         expected(expected<U, G>&& o)
             noexcept(std::is_nothrow_constructible_v<T, U> &&
                      std::is_nothrow_constructible_v<E, G>);
/*(6)*/  template<class U = T>
         requires std::is_constructible_v<T, U>
         explicit(!std::is_convertible_v<U, T>)
         expected(U&& v) noexcept(std::is_nothrow_constructible_v<T, U>);
/*(7)*/  template<class G>
         requires std::is_constructible_v<E, const G&>
         explicit(!std::is_convertible_v<const G&, E>)
         expected(const unexpected<G>& u) noexcept(std::is_nothrow_constructible_v<E, const G&>);
/*(8)*/  template<class G>
         requires std::is_constructible_v<E, G>
         explicit(!std::is_convertible_v<G, E>)
         expected(unexpected<G>&& u) noexcept(std::is_nothrow_constructible_v<E, G>);
/*(9)*/  template<class... A>
         requires std::is_constructible_v<T, A...>
         explicit expected(std::in_place_t, A&&... a)
             noexcept(std::is_nothrow_constructible_v<T, A...>);
/*(10)*/ template<class U, class... A>
         requires std::is_constructible_v<T, std::initializer_list<U>&, A...>
         explicit expected(std::in_place_t, std::initializer_list<U> il, A&&... a)
             noexcept(std::is_nothrow_constructible_v<T, std::initializer_list<U>&, A...>);
/*(11)*/ template<class... A>
         requires std::is_constructible_v<E, A...>
         explicit expected(unexpect_t, A&&... a) noexcept(std::is_nothrow_constructible_v<E, A...>);
/*(12)*/ template<class U, class... A>
         requires std::is_constructible_v<E, std::initializer_list<U>&, A...>
         explicit expected(unexpect_t, std::initializer_list<U> il, A&&... a)
             noexcept(std::is_nothrow_constructible_v<E, std::initializer_list<U>&, A...>);
```

Constructs an `expected` with a value or an error.

1. A value-initialized value.
2. A copy of the value or the error of the other `expected`.
3. The value or the error of the other `expected`, moved; the other keeps a moved-from one.
4. The value of `o` converted to `T`, or its error converted to `E`.
5. The same, moved from `o`.
6. The value `T(std::forward<U>(v))`. Takes part only when `U` is not `std::in_place_t`, an `expected` or an
   `unexpected`.
7. The error, `E` from `u.error()`.
8. The same, moved from `u`.
9. The value, constructed from `a...`.
10. The value, constructed from `il` and `a...`.
11. The error, constructed from `a...`.
12. The error, constructed from `il` and `a...`.

- (2–3) Each is as noexcept as the copy or the move of `T` and `E`.
- (4–5) Take part only when `T` cannot be made from the other `expected` as a whole, nor `unexpected<E>`, as
  `std::expected` asks. For a `bool` value the first check is not made (LWG 3836): every `expected` would convert
  to `bool`, so `expected<bool, int>(expected<int, int>(0))` holds `false`, the value converted, not `true`.
- (4–8) `explicit` when the conversion of the value or the error is.

`expected<void, E>` has (1)–(3), (7), (8), (11), (12), the conversions (4)–(5) from another `expected<void, G>`,
and `explicit expected(std::in_place_t) noexcept`: a success.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the `expected` whose value or error is converted |
| `v` | the value |
| `u` | the error, wrapped in an `unexpected` |
| `a` | the arguments the value or the error is constructed from |
| `il` | the initializer list the value or the error is constructed from |

## Complexity

Constant, plus the construction of the value or the error.

## Exceptions

What the constructor, the copy or the move of `T` or of `E` throws; none when the one that runs is noexcept.

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
    expected<int, string> zero;
    expected<tracked_ptr<Node>, string> found = make_tracked<Node>(1);
    expected<tracked_ptr<Node>, string> missing = unexpected("no node");
    expected<vector<int>, string> list(std::in_place, {1, 2, 3});
    expected<int, string> failed(unexpect, 3, '!');
    expected<long, string> wider = zero;

    println("{} {} {}", *zero, (*found)->value, missing.error());
    println("{} {} {}", *list, failed.error(), *wider);
}
```

Output:

```text
0 1 no node
[1, 2, 3] !!! 0
```

## See also

- [operator=](operator_assign.md): assigns another `expected`, a value or an error
- [unexpected](../unexpected.md): the error, wrapped
- [sgcl::expected\<T, E\>](../expected.md)

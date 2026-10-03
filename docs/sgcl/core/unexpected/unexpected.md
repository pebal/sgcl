[sgcl](../../README.md) › [core](../README.md) › [unexpected](../unexpected.md)

# sgcl::unexpected\<E\>::unexpected

```cpp
unexpected(const unexpected&) = default;                                              // (1)
unexpected(unexpected&&) = default;                                                   // (2)
template<class Err = E>
requires std::is_constructible_v<E, Err>
explicit unexpected(Err&& e) noexcept(std::is_nothrow_constructible_v<E, Err>);       // (3)
template<class... A>
requires std::is_constructible_v<E, A...>
explicit unexpected(std::in_place_t, A&&... a)                                        // (4)
    noexcept(std::is_nothrow_constructible_v<E, A...>);
template<class U, class... A>
requires std::is_constructible_v<E, std::initializer_list<U>&, A...>
explicit unexpected(std::in_place_t, std::initializer_list<U> il, A&&... a)           // (5)
    noexcept(std::is_nothrow_constructible_v<E, std::initializer_list<U>&, A...>);
```

Constructs an `unexpected`.

1. A copy of the error of the other.
2. The error of the other, moved.
3. The error `E(std::forward<Err>(e))`. Takes part only when `Err` is not an `unexpected`, `std::in_place_t` or an
   `expected`.
4. The error, constructed from `a...`.
5. The error, constructed from `il` and `a...`.

There is no default constructor: an `unexpected` always holds an error.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the value the error is made from |
| `a` | the arguments the error is constructed from |
| `il` | the initializer list the error is constructed from |

## Complexity

Constant, plus the construction of the error.

## Exceptions

What the constructor, the copy or the move of `E` throws; none when the one that runs is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <utility>

using namespace sgcl;

int main() {
    unexpected<int> code(404);
    unexpected<string> text(std::in_place, 3, '?');
    unexpected deduced("plain");  // unexpected<const char*>

    expected<int, string> e = text;
    println("{} {} {} {}", code.error(), text.error(), deduced.error(), e.error());
}
```

Output:

```text
404 ??? plain ???
```

## See also

- [error](error.md): the error
- [sgcl::unexpected\<E\>](../unexpected.md)

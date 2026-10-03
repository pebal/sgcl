[sgcl](../../README.md) › [core](../README.md) › [expected](../expected.md)

# sgcl::operator== (sgcl::expected)

```cpp
/*(1)*/ template<class T2, class E2>
        requires (!std::is_void_v<T2>)
        friend bool operator==(const expected& x, const expected<T2, E2>& y)
            noexcept(noexcept(bool(std::declval<const T&>() == std::declval<const T2&>())) &&
                     noexcept(bool(x.error() == y.error())));
/*(2)*/ template<class T2>
        friend bool operator==(const expected& x, const T2& v)
            noexcept(noexcept(bool(std::declval<const T&>() == v)));
/*(3)*/ template<class E2>
        friend bool operator==(const expected& x, const unexpected<E2>& e)
            noexcept(noexcept(bool(x.error() == e.error())));
```

Compares an `expected` with another, with a value or with an error. Hidden friends: found by the argument's type
alone. `!=` is the negation of each, rewritten by the compiler.

1. `true` when both hold a value and the values are equal, or both hold an error and the errors are equal.
2. `true` when `x` holds a value equal to `v`. Takes part only when `T2` is not an `expected` or an `unexpected`.
3. `true` when `x` holds an error equal to `e.error()`.

`expected<void, E>` has (1) against another `expected<void, E2>`, where two successes are equal, and (3).

## Parameters

| Parameter | Description |
|---|---|
| `x`, `y` | the `expected` objects to compare |
| `v` | the value to compare with |
| `e` | the error to compare with |

## Return value

The result of the comparison.

## Complexity

Constant, plus one comparison of the values or the errors.

## Exceptions

What the comparison of the values or the errors throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    expected<int, string> a = 1, b = 1;
    expected<int, string> failed = unexpected("no");
    println("{} {} {}", a == b, a == 1, a != failed);
    println("{} {}", failed == unexpected(string("no")), failed == 1);
}
```

Output:

```text
true true true
true false
```

## See also

- [operator bool, has_value](operator_bool.md): checks whether there is a value
- [sgcl::expected\<T, E\>](../expected.md)

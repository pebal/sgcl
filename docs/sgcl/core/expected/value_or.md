[sgcl](../../README.md) › [core](../README.md) › [expected](../expected.md)

# sgcl::expected\<T, E\>::value_or

```cpp
/*(1)*/ template<class U>
        T value_or(U&& v) const&
            noexcept(std::is_nothrow_copy_constructible_v<T> &&
                     std::is_nothrow_constructible_v<T, U>);
/*(2)*/ template<class U>
        T value_or(U&& v) &&
            noexcept(std::is_nothrow_move_constructible_v<T> &&
                     std::is_nothrow_constructible_v<T, U>);
```

The value when there is one, else `v` converted to `T`.

1. The value copied.
2. The value moved out of the `expected`.

`expected<void, E>` has no `value_or`.

## Parameters

| Parameter | Description |
|---|---|
| `v` | the value to return when there is none |

## Return value

The value, or `static_cast<T>(std::forward<U>(v))`.

## Complexity

Constant, plus the copy, the move or the conversion.

## Exceptions

What the copy (1) or the move (2) of `T`, or the construction of `T` from `v`, throws; none when they are noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int value;
};

int main() {
    expected<int, string> port = unexpected("no port given");
    println("{}", port.value_or(8080));

    expected<tracked_ptr<Node>, string> found = unexpected("no node");
    tracked_ptr<Node> node = found.value_or(nullptr);
    println("{}", node == nullptr);
}
```

Output:

```text
8080
true
```

## See also

- [error_or](error_or.md): the error, or another one when there is none
- [value, operator U](value.md): the value, `bad_expected_access` without one
- [sgcl::expected\<T, E\>](../expected.md)

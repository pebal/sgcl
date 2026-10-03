[sgcl](../../README.md) › [core](../README.md) › [expected](../expected.md)

# sgcl::expected\<T, E\>::error_or

```cpp
/*(1)*/ template<class G = E>
        E error_or(G&& e) const&
            noexcept(std::is_nothrow_copy_constructible_v<E> &&
                     std::is_nothrow_constructible_v<E, G>);
/*(2)*/ template<class G = E>
        E error_or(G&& e) &&
            noexcept(std::is_nothrow_move_constructible_v<E> &&
                     std::is_nothrow_constructible_v<E, G>);
```

The error when there is one, else `e` converted to `E`.

1. The error copied.
2. The error moved out of the `expected`.

`expected<void, E>` has both.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the error to return when there is a value |

## Return value

The error, or `static_cast<E>(std::forward<G>(e))`.

## Complexity

Constant, plus the copy, the move or the conversion.

## Exceptions

What the copy (1) or the move (2) of `E`, or the construction of `E` from `e`, throws; none when they are noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    expected<int, string> ok = 1;
    expected<int, string> failed = unexpected("timeout");
    println("[{}] [{}]", ok.error_or(""), failed.error_or(""));

    expected<void, int> done;
    println("{}", done.error_or(0));
}
```

Output:

```text
[] [timeout]
0
```

## See also

- [value_or](value_or.md): the value, or another one when there is none
- [error](error.md): the error
- [sgcl::expected\<T, E\>](../expected.md)

[sgcl](../../README.md) › [core](../README.md) › [expected](../expected.md)

# sgcl::swap (sgcl::expected)

```cpp
friend void swap(expected& x, expected& y) noexcept(noexcept(x.swap(y)))
    requires requires { x.swap(y); };
```

Swaps the contents of `x` and `y`: `x.swap(y)` ([swap](swap.md)). A hidden friend: found by the argument's type
alone, by `swap(a, b)` written without a namespace and by the `using std::swap; swap(a, b);` of generic code. Takes
part only when the member `swap` does.

## Parameters

| Parameter | Description |
|---|---|
| `x`, `y` | the `expected` objects to swap |

## Return value

None.

## Complexity

Constant, plus the swap or the moves of the values and the errors.

## Exceptions

What the move constructor or the swap of `T` or of `E` throws; none when they are noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    expected<string, int> a = "text";
    expected<string, int> b = unexpected(404);
    swap(a, b);
    println("{} {}", a.error(), *b);
}
```

Output:

```text
404 text
```

## See also

- [swap](swap.md): the member function
- [sgcl::expected\<T, E\>](../expected.md)

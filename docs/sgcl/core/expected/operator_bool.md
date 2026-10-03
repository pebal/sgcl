[sgcl](../../README.md) › [core](../README.md) › [expected](../expected.md)

# sgcl::expected\<T, E\>::operator bool, has_value

```cpp
explicit operator bool() const noexcept;    // (1)
bool has_value() const noexcept;            // (2)
```

Checks whether the `expected` holds a value rather than an error. `if (e)` asks this, for an `expected<bool, E>` as
well: the conversion to the value is never a conversion to `bool`.

## Parameters

None.

## Return value

`true` when there is a value, `false` when there is an error.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

expected<bool, string> is_even(int n) {
    if (n < 0) {
        return unexpected("negative");
    }
    return n % 2 == 0;
}

int main() {
    auto a = is_even(3);
    if (a) {  // there is a value, though the value is false
        println("{} {}", a.has_value(), *a);
    }
    auto b = is_even(-1);
    println("{} {}", b.has_value(), b.error());
}
```

Output:

```text
true false
false negative
```

## See also

- [value, operator U](value.md): the value, `bad_expected_access` without one
- [error](error.md): the error
- [sgcl::expected\<T, E\>](../expected.md)

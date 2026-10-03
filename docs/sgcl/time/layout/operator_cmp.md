[sgcl](../../README.md) › [time](../README.md) › [layout](../layout.md)

# sgcl::time::operator== (sgcl::time::layout)

```cpp
friend constexpr bool operator==(layout, layout) noexcept = default;
```

Compares two layouts: equal when they are the same format, one of the five constants. `!=` is made from it by the
compiler. It is how a layout held in a variable, a setting of the program, is told apart.

## Parameters

The two layouts compared, the operands, unnamed in the declaration.

## Return value

`true` when the two are the same layout.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::layout format = time::http;
    println("{} {}", format == time::http, format == time::email);
    println("{}", time::rfc3339 != time::rfc3339_nano);
}
```

Output:

```text
true false
true
```

## See also

- [sgcl::time::layout](../layout.md)

[sgcl](../../README.md) › [core](../README.md) › [any](README.md)

# sgcl::any::has_value

```cpp
bool has_value() const noexcept;
```

Checks whether the `any` holds a value. An `any` is empty when default-constructed, moved from, after
[reset](reset.md), and after an [emplace](emplace.md) whose construction threw.

## Parameters

None.

## Return value

`true` when the `any` holds a value, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <utility>

using namespace sgcl;

int main() {
    any a;
    println("{}", a.has_value());
    a = 3;
    println("{}", a.has_value());
    any b = std::move(a);
    println("{} {}", a.has_value(), b.has_value());
}
```

Output:

```text
false
true
false true
```

## See also

- [type](type.md): the type of the value held
- [sgcl::any](README.md)

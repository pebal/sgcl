[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::is_bool

```cpp
bool is_bool() const noexcept;
```

Whether the value is a boolean: one question of [type](type.md).

## Parameters

None.

## Return value

`true` when it is.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto v = encoding::toml::parse("a = [{}, [1], 'x', 7, 2.5, true, 07:32:00]").value();
    for (const auto& e : v["a"].elements()) {
        print("{} ", e.is_bool());
    }
    println();
}
```

Output:

```text
false false false false false true false 
```

## See also

- [type](type.md)
- [sgcl::encoding::toml](README.md)

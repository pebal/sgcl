[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::empty

```cpp
bool empty() const noexcept;
```

Whether [size](size.md) is 0: an empty array or table, and every scalar.

## Parameters

None.

## Return value

`true` when there is nothing inside.

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
    println("{} {} {}", encoding::toml().empty(), encoding::toml::parse("a = 1")->empty(), encoding::toml(1).empty());
}
```

Output:

```text
true false true
```

## See also

- [size](size.md)
- [sgcl::encoding::toml](README.md)

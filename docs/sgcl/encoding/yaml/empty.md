[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::empty

```cpp
bool empty() const noexcept;
```

Whether [size](size.md) is 0: an empty collection, and every scalar.

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
    println("{} {} {}", encoding::yaml::parse("[]")->empty(), encoding::yaml::parse("[1]")->empty(), encoding::yaml(1).empty());
}
```

Output:

```text
true false true
```

## See also

- [size](size.md)
- [sgcl::encoding::yaml](README.md)

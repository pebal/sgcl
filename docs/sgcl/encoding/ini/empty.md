[sgcl](../../README.md) › [encoding](../README.md) › [ini](README.md)

# sgcl::encoding::ini::empty

```cpp
bool empty() const noexcept;
```

Whether there is no section.

## Parameters

None.

## Return value

`true` when there is none.

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
    println("{} {}", encoding::ini::parse("; nothing\n")->empty(), encoding::ini::parse("[a]")->empty());
}
```

Output:

```text
true false
```

## See also

- [size](size.md)
- [sgcl::encoding::ini](README.md)

[sgcl](../../README.md) › [txt](../README.md) › [percent_set](../percent_set.md)

# sgcl::txt::percent_set::holds

```cpp
constexpr bool holds(char c) const noexcept;
```

Checks whether the character `c` is in the set: whether an encoding with it leaves `c` alone. A byte above ASCII is never in a set.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the character, a byte |

## Return value

`true` when `c` is in the set.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto& path = txt::percent::path;
    println("{} {} {}", path.holds('/'), txt::percent::segment.holds('/'), path.holds(char(0xC3)));
}
```

Output:

```text
true false false
```

## See also

- [sgcl::txt::percent_set](../percent_set.md)

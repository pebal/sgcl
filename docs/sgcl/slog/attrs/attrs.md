[sgcl](../../README.md) › [slog](../README.md) › [attrs](README.md)

# sgcl::slog::attrs::attrs

```cpp
attrs() noexcept;
```

Constructs an empty range, of no attributes. The ranges with attributes are made by the module:
[value::as_group](../value/as_group.md) returns one.

## Parameters

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::attrs none;
    println("{} {}", none.empty(), none.size());
}
```

Output:

```text
true 0
```

## See also

- [value::as_group](../value/as_group.md)
- [sgcl::slog::attrs](README.md)

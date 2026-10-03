[sgcl](../../README.md) › [slog](../README.md) › [attr](../attr.md)

# sgcl::slog::attr::key

```cpp
slice<const char> key() const noexcept;
```

Returns the key of the attribute: the literal or the `const char*` the call gave, or a group's name.

## Parameters

None.

## Return value

A slice of the key.

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
    slog::memory kept;
    const char* name = "user";
    slog::logger(kept).group("req").info("m", name, "ala");
    auto top = *kept.records()[0].begin();
    println("{} {}", top.key(), (*top.value().as_group().begin()).key());
}
```

Output:

```text
req user
```

## See also

- [value](value.md)
- [sgcl::slog::attr](../attr.md)

[sgcl](../../README.md) › [slog](../README.md) › [attrs](README.md)

# sgcl::slog::attrs::empty

```cpp
bool empty() const noexcept;
```

Checks whether the range has no attribute: a group written with none, or a range made by the default constructor.

## Parameters

None.

## Return value

`true` when there is no attribute, `false` otherwise.

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
    slog::logger(kept).info("m", slog::group("none"), slog::group("one", "a", 1));
    for (auto a : kept.records()[0]) {
        println("{}: {}", a.key(), a.value().as_group().empty());
    }
}
```

Output:

```text
none: true
one: false
```

## See also

- [size](size.md)
- [sgcl::slog::attrs](README.md)

[sgcl](../../README.md) › [slog](../README.md) › [journald](README.md)

# sgcl::slog::operator==, operator!= (sgcl::slog::journald)

```cpp
friend bool operator==(const journald& a, const journald& b) noexcept;
```

Checks whether two handles stand for the same handler. The `!=` is the one C++ writes from this `==`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when `a` and `b` are the same.

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
    auto journal = slog::journald::open();
    if (journal) {
        slog::journald copy = *journal;
        println("{}", copy == *journal);
    }
}
```

## See also

- [open](open.md)
- [sgcl::slog::journald](README.md)

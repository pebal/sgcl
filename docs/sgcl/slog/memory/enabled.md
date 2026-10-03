[sgcl](../../README.md) › [slog](../README.md) › [memory](README.md)

# sgcl::slog::memory::enabled

```cpp
bool enabled(slog::level) const noexcept;
```

Returns `true` for every level: a keeper takes every record its logger writes, and the logger's own level decides
which those are.

## Parameters

| Parameter | Description |
|---|---|
| (unnamed) | the level asked about, not read |

## Return value

`true`.

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
    println("{} {}", kept.enabled(slog::level(-100)),
            slog::logger(kept).enabled(slog::level::debug));
}
```

Output:

```text
true false
```

## See also

- [handle](handle.md)
- [logger::enabled](../logger/enabled.md)
- [sgcl::slog::memory](README.md)

[sgcl](../../README.md) › [slog](../README.md) › [level_var](README.md)

# sgcl::slog::level_var::get

```cpp
level get() const noexcept;
```

Returns the level now, slog's `LevelVar.Level`: the last one [set](set.md) on this variable or a copy of it, on any
thread, or the one it was made at (a relaxed load).

## Parameters

None.

## Return value

The level.

## Complexity

Constant: one relaxed atomic load.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::level_var least;
    println("{}", int(least.get()));
    least.set(slog::level(6));
    println("{}", int(least.get()));
}
```

Output:

```text
0
6
```

## See also

- [set](set.md)
- [sgcl::slog::level_var](README.md)
